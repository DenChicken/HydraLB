import std;
import hydralb.dpdk;
import hydralb.data;
import hydralb.common.config;

namespace {

using namespace hydralb::config;

const std::string MBUF_POOL_NAME = "HYDRALB_MBUF_POOL";

AppConfig make_app_config() {
    AppConfig config;

    config.profile.mode = PipelineMode::PcapLoadBalancer;

    config.environment.eal_args = {"HydraLB", "-c", "0xf", "-n", "4", "--no-huge"};

    config.memory.mempools = {
        MempoolConfig{
            .name = MBUF_POOL_NAME,
            .num_elements = 8191,
            .cache_size = 256,
            .socket_id = -1,
        },
    };

    config.threading.worker_lcores = {1};

    config.nodes.pcap_ingress = {
        .filename = "pcap/https-and-dns-to-google.com.pcapng",
        .mempool_name = MBUF_POOL_NAME};
    config.nodes.pcap_egress = {.filename = "output.pcap"};

    return config;
}

}  // namespace

int main() {
    const AppConfig config = make_app_config();

    auto init_ok = hydralb::dpdk::Eal::init(config.environment.eal_args);
    if (!init_ok) {
        std::println(std::cerr, "Error: {}", init_ok.error());
        return 1;
    }

    auto memory_ok = hydralb::data::setup_memory(config.memory);
    if (!memory_ok) {
        std::println(std::cerr, "Error: {}", memory_ok.error());
        return 1;
    }

    std::println("DPDK EAL and Mempool partitions loaded successfully");

    auto run_ok = hydralb::data::Dispatcher::run(config);
    if (!run_ok) {
        std::println(std::cerr, "Error: {}", run_ok.error());
        return 1;
    }

    std::println("Pipeline finished. Cleaning up...");

    hydralb::dpdk::Eal::cleanup();

    return 0;
}
