module;

#include <rte_errno.h>
#include <rte_launch.h>
#include <rte_lcore.h>

export module hydralb.data:dispatcher;

import :bootstrap;
import :pipeline;
import :pcap_ingress;
import :pcap_egress;
import :lb;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::size_t IDLE_BURSTS_BEFORE_STOP = 128;
constexpr std::size_t SUPPORTED_WORKERS = 1;

using PcapPassthroughPipeline = Pipeline<PcapIngressNode, PcapEgressNode>;

using PcapLoadBalancerPipeline = Pipeline<PcapIngressNode, LBNode, PcapEgressNode>;

struct WorkerContext {
    const config::AppConfig* config = nullptr;
    const config::RoutingTable* routing_table = nullptr;
    std::vector<NodeStats>* stats = nullptr;
    std::uint32_t lcore_id = 0;
};

static PcapPassthroughPipeline make_passthrough_pipeline(const config::AppConfig& config) {
    return PcapPassthroughPipeline{
        PcapIngressNode{config.nodes.pcap_ingress},
        PcapEgressNode{config.nodes.pcap_egress},
    };
}

static PcapLoadBalancerPipeline make_pcap_lb_pipeline(
    const config::AppConfig& config,
    const config::RoutingTable& rt) {
    LBNode::Config lb_config{
        .routing_table = &rt,
        .local_tunnel_ip = config.balancing.local_tunnel_ip.address,
        .flow_hash_seed = config.balancing.flow_hash_seed};

    return PcapLoadBalancerPipeline{
        PcapIngressNode{config.nodes.pcap_ingress},
        LBNode{lb_config},
        PcapEgressNode{config.nodes.pcap_egress},
    };
}

template <typename Pipeline>
static void worker_loop(Pipeline pipeline, std::vector<NodeStats>& stats) {
    auto configure_ok = pipeline.configure();
    if (!configure_ok) {
        std::println(std::cerr, "Worker configure failed: {}", configure_ok.error());
        return;
    }

    std::array<dpdk::Packet, config::BURST_SIZE> batch{};

    std::size_t idle_bursts = 0;

    while (idle_bursts < IDLE_BURSTS_BEFORE_STOP) {
        if (pipeline.process(batch).empty()) {
            ++idle_bursts;
        } else {
            idle_bursts = 0;
        }
    }

    stats = pipeline.collect_stats();
    pipeline.shutdown();
}

}  // namespace hydralb::data

extern "C" {

static int worker_entry(void* arg) {
    auto* ctx = static_cast<hydralb::data::WorkerContext*>(arg);
    const auto& config = *ctx->config;
    const auto& rt = *ctx->routing_table;

    ctx->lcore_id = ::rte_lcore_id();

    switch (config.mode) {
        case hydralb::config::PipelineMode::PcapPassthrough:
            hydralb::data::worker_loop(
                hydralb::data::make_passthrough_pipeline(config),
                *ctx->stats);
            break;
        case hydralb::config::PipelineMode::PcapLoadBalancer:
            hydralb::data::worker_loop(
                hydralb::data::make_pcap_lb_pipeline(config, rt),
                *ctx->stats);
            break;
        default:
            break;
    }

    return 0;
}

}  // extern "C"

export namespace hydralb::data {

class Dispatcher {
public:
    static std::expected<void, std::string> run(
        const config::AppConfig& config,
        const config::RoutingTable& routing_table) {
        const auto& lcores = config.threading.worker_lcores;
        if (lcores.empty()) {
            return std::unexpected("No worker lcores configured");
        }
        if (lcores.size() > SUPPORTED_WORKERS) {
            return std::unexpected(
                std::format(
                    "Worker lcore count {} exceeds supported workers {}",
                    lcores.size(),
                    SUPPORTED_WORKERS));
        }

        std::vector<WorkerContext> contexts(lcores.size());
        std::vector<std::vector<NodeStats>> stats(lcores.size());
        std::expected<void, std::string> launch_result;

        std::size_t launched = 0;

        for (std::size_t i = 0; i < lcores.size(); ++i) {
            contexts[i] = WorkerContext{&config, &routing_table, &stats[i], 0};

            int launch_ret = ::rte_eal_remote_launch(worker_entry, &contexts[i], lcores[i]);
            if (launch_ret < 0) {
                launch_result = std::unexpected(
                    std::format(
                        "Failed to launch worker on lcore {}: {}",
                        lcores[i],
                        ::rte_strerror(-launch_ret)));
                break;
            }

            ++launched;
        }

        ::rte_eal_mp_wait_lcore();

        for (std::size_t i = 0; i < launched; ++i) {
            print_stats(contexts[i].lcore_id, stats[i]);
        }

        return launch_result;
    }
};

}  // namespace hydralb::data
