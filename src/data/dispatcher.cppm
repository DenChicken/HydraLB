module;

#include <rte_launch.h>
#include <rte_lcore.h>

export module hydralb.data:dispatcher;

import :pipeline;
import :pcap_ingress;
import :pcap_egress;
import hydralb.common.app_config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::size_t WORKER_BATCH_SIZE = 32;

using PcapPassthroughPipeline = Pipeline<1, PcapIngressNode, PcapEgressNode>;

struct WorkerContext {
    const config::AppConfig* config = nullptr;
    std::size_t worker_index = 0;
};

static PcapPassthroughPipeline make_passthrough_pipeline(const config::AppConfig& config) {
    return PcapPassthroughPipeline{
        PcapIngressNode{config.nodes.pcap_ingress},
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

    while (true) {
        pipeline.process(batch);
    }
}

}  // namespace hydralb::data

extern "C" {

static int worker_entry(void* arg) {
    auto* ctx = static_cast<hydralb::data::WorkerContext*>(arg);
    const auto& config = *ctx->config;

    switch (config.profile.mode) {
        case hydralb::config::PipelineMode::PcapPassthrough:
            hydralb::data::worker_loop(hydralb::data::make_passthrough_pipeline(config));
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

        std::vector<WorkerContext> contexts(lcores.size());
        for (std::size_t i = 0; i < lcores.size(); ++i) {
            contexts[i] = WorkerContext{&config, i};

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
