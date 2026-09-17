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

using PcapPassthroughPipeline = Pipeline<PcapIngressNode, PcapEgressNode>;

using PcapLoadBalancerPipeline = Pipeline<PcapIngressNode, LBNode, PcapEgressNode>;

struct WorkerContext {
    const config::WorkerConfig* worker = nullptr;
    const config::RuntimeConfig* runtime = nullptr;
    const config::RoutingTable* routing_table = nullptr;
    std::vector<NodeStats>* stats = nullptr;
    std::uint32_t lcore_id = 0;
};

static PcapIngressNode::Config make_ingress_config(const config::WorkerConfig& worker) {
    return PcapIngressNode::Config{
        .device_name = worker.rx.device,
        .mempool_name = worker.mempool,
        .queue_id = worker.rx.queue};
}

static PcapEgressNode::Config make_egress_config(const config::WorkerConfig& worker) {
    return PcapEgressNode::Config{.device_name = worker.tx.device, .queue_id = worker.tx.queue};
}

static PcapPassthroughPipeline make_passthrough_pipeline(const config::WorkerConfig& worker) {
    return PcapPassthroughPipeline{
        PcapIngressNode{make_ingress_config(worker)},
        PcapEgressNode{make_egress_config(worker)},
    };
}

static PcapLoadBalancerPipeline make_pcap_lb_pipeline(
    const config::WorkerConfig& worker,
    const config::RuntimeConfig& runtime,
    const config::RoutingTable& rt) {
    LBNode::Config lb_config{
        .routing_table = &rt,
        .local_tunnel_ip = runtime.local_tunnel_ip.address,
        .flow_hash_seed = runtime.flow_hash_seed};

    return PcapLoadBalancerPipeline{
        PcapIngressNode{make_ingress_config(worker)},
        LBNode{lb_config},
        PcapEgressNode{make_egress_config(worker)},
    };
}

template <typename Pipeline>
static void worker_loop(Pipeline pipeline, std::vector<NodeStats>& stats) {
    auto configure_ok = pipeline.configure();
    if (!configure_ok) {
        std::println(std::cerr, "Worker configure failed: {}", configure_ok.error());
        return;
    }

    const auto& stop = dpdk::Signals::stop_flag();

    std::array<dpdk::Packet, config::BURST_SIZE> batch{};

    while (!stop.load(std::memory_order_relaxed)) {
        pipeline.process(batch);
    }

    stats = pipeline.collect_stats();
    pipeline.shutdown();
}

}  // namespace hydralb::data

extern "C" {

static int worker_entry(void* arg) {
    auto* ctx = static_cast<hydralb::data::WorkerContext*>(arg);
    const auto& worker = *ctx->worker;
    const auto& runtime = *ctx->runtime;
    const auto& rt = *ctx->routing_table;

    ctx->lcore_id = ::rte_lcore_id();

    switch (worker.mode) {
        case hydralb::config::PipelineMode::PcapPassthrough:
            hydralb::data::worker_loop(
                hydralb::data::make_passthrough_pipeline(worker),
                *ctx->stats);
            break;
        case hydralb::config::PipelineMode::PcapLoadBalancer:
            hydralb::data::worker_loop(
                hydralb::data::make_pcap_lb_pipeline(worker, runtime, rt),
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
        const config::StartupConfig& startup,
        const config::RuntimeConfig& runtime,
        const config::RoutingTable& routing_table) {
        const auto& workers = startup.workers;
        if (workers.empty()) {
            return std::unexpected("No workers configured");
        }

        std::vector<WorkerContext> contexts(workers.size());
        std::vector<std::vector<NodeStats>> stats(workers.size());
        std::expected<void, std::string> launch_result;

        std::size_t launched = 0;

        for (std::size_t i = 0; i < workers.size(); ++i) {
            contexts[i] = WorkerContext{&workers[i], &runtime, &routing_table, &stats[i], 0};

            int launch_ret = ::rte_eal_remote_launch(worker_entry, &contexts[i], workers[i].lcore);
            if (launch_ret < 0) {
                launch_result = std::unexpected(
                    std::format(
                        "Failed to launch worker on lcore {}: {}",
                        workers[i].lcore,
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
