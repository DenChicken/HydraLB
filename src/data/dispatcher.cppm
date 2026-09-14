module;

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

constexpr std::size_t WORKER_BATCH_SIZE = 32;
constexpr std::uint32_t LOCAL_TUNNEL_IP = 0x0A000001;

constexpr std::size_t IDLE_BURSTS_BEFORE_STOP = 128;

using PcapPassthroughPipeline = Pipeline<1, PcapIngressNode, PcapEgressNode>;

using PcapLoadBalancerPipeline = Pipeline<1, PcapIngressNode, LBNode, PcapEgressNode>;

struct WorkerContext {
    const config::AppConfig* config = nullptr;
    const config::RoutingTable* routing_table = nullptr;
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
        .local_tunnel_ip = LOCAL_TUNNEL_IP,
        .lcore_id = ::rte_lcore_id()};

    return PcapLoadBalancerPipeline{
        PcapIngressNode{config.nodes.pcap_ingress},
        LBNode{lb_config},
        PcapEgressNode{config.nodes.pcap_egress},
    };
}

template <typename Pipeline>
static void worker_loop(Pipeline pipeline) {
    auto configure_ok = pipeline.configure();
    if (!configure_ok) {
        std::println(std::cerr, "Worker configure failed: {}", configure_ok.error());
        return;
    }

    std::array<dpdk::Packet, WORKER_BATCH_SIZE> batch{};

    std::size_t idle_bursts = 0;

    while (idle_bursts < IDLE_BURSTS_BEFORE_STOP) {
        if (pipeline.process(batch).empty()) {
            ++idle_bursts;
        } else {
            idle_bursts = 0;
        }
    }

    pipeline.dump_stats();
}

}  // namespace hydralb::data

extern "C" {

static int worker_entry(void* arg) {
    auto* ctx = static_cast<hydralb::data::WorkerContext*>(arg);
    const auto& config = *ctx->config;
    const auto& rt = *ctx->routing_table;

    switch (config.profile.mode) {
        case hydralb::config::PipelineMode::PcapPassthrough:
            hydralb::data::worker_loop(hydralb::data::make_passthrough_pipeline(config));
            break;
        case hydralb::config::PipelineMode::PcapLoadBalancer:
            hydralb::data::worker_loop(hydralb::data::make_pcap_lb_pipeline(config, rt));
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
    static std::expected<void, std::string> run(const config::AppConfig& config) {
        std::size_t max_workers = 0;
        switch (config.profile.mode) {
            case config::PipelineMode::PcapPassthrough:
                max_workers = PcapPassthroughPipeline::max_workers;
                break;
            case config::PipelineMode::PcapLoadBalancer:
                max_workers = PcapLoadBalancerPipeline::max_workers;
                break;
            default:
                return std::unexpected("Invalid profile mode");
        }

        const auto& lcores = config.threading.worker_lcores;
        if (lcores.empty()) {
            return std::unexpected("No worker lcores configured");
        }
        if (lcores.size() > max_workers) {
            return std::unexpected(
                std::format(
                    "Worker lcore count {} exceeds pipeline max workers {}",
                    lcores.size(),
                    max_workers));
        }

        static config::RoutingTable routing_table;

        auto routing_ok = setup_routing(config.balancing, routing_table);
        if (!routing_ok) {
            return std::unexpected(routing_ok.error());
        }

        std::vector<WorkerContext> contexts(lcores.size());
        for (std::size_t i = 0; i < lcores.size(); ++i) {
            contexts[i] = WorkerContext{&config, &routing_table};

            int launch_ret = ::rte_eal_remote_launch(worker_entry, &contexts[i], lcores[i]);
            if (launch_ret < 0) {
                return std::unexpected(
                    std::format("Failed to launch worker on lcore {}: {}", lcores[i], launch_ret));
            }
        }

        ::rte_eal_mp_wait_lcore();

        return {};
    }
};

}  // namespace hydralb::data
