import std;
import hydralb.dpdk;
import hydralb.data;
import hydralb.common.config;

namespace {

using namespace hydralb::config;

const std::string MBUF_POOL_NAME = "HYDRALB_MBUF_POOL";

constexpr std::uint32_t MEMPOOL_ELEMENTS = 8191;
constexpr std::uint32_t MEMPOOL_CACHE_SIZE = 256;
constexpr std::int32_t MEMPOOL_SOCKET_ID = -1;

constexpr std::uint32_t LOCAL_TUNNEL_IP = 0x0A000001;
constexpr std::uint32_t FLOW_HASH_SEED = 0x5BD1E995;

constexpr std::uint32_t BACKEND_IP_BASE = 0x0A00000A;
constexpr std::uint16_t BACKEND_PORT = 80;
constexpr std::uint32_t BACKEND_WEIGHT = 1;
constexpr std::size_t BACKEND_COUNT = 4;

constexpr std::uint32_t WORKER_LCORE = 1;

AppConfig make_app_config() {
    AppConfig config;

    config.profile.mode = PipelineMode::PcapLoadBalancer;

    config.environment.eal_args = {"HydraLB", "-c", "0xf", "-n", "4", "--no-huge"};

    config.memory.mempools = {
        MempoolConfig{
            .name = MBUF_POOL_NAME,
            .num_elements = MEMPOOL_ELEMENTS,
            .cache_size = MEMPOOL_CACHE_SIZE,
            .socket_id = MEMPOOL_SOCKET_ID,
        },
    };

    config.threading.worker_lcores = {WORKER_LCORE};

    config.balancing.local_tunnel_ip = {.address = LOCAL_TUNNEL_IP};
    config.balancing.flow_hash_seed = FLOW_HASH_SEED;

    for (std::size_t i = 0; i < BACKEND_COUNT; ++i) {
        config.balancing.backends.push_back(
            Backend{
                .id = static_cast<std::uint32_t>(i + 1),
                .ip = {.address = BACKEND_IP_BASE + static_cast<std::uint32_t>(i)},
                .port = BACKEND_PORT,
                .weight = BACKEND_WEIGHT,
                .status = BackendStatus::Alive});
    }

    config.nodes.pcap_ingress = {
        .device_name = "net_pcap_ingress",
        .filename = "pcap/https-and-dns-to-google.com.pcapng",
        .mempool_name = MBUF_POOL_NAME};
    config.nodes.pcap_egress = {.device_name = "net_pcap_egress", .filename = "output.pcap"};

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

    RoutingTable routing_table;

    auto routing_ok = hydralb::data::setup_routing(config.balancing, routing_table);
    if (!routing_ok) {
        std::println(std::cerr, "Error: {}", routing_ok.error());
        return 1;
    }

    std::println("DPDK EAL, mempools and routing table are ready");

    auto run_ok = hydralb::data::Dispatcher::run(config, routing_table);
    if (!run_ok) {
        std::println(std::cerr, "Error: {}", run_ok.error());
        return 1;
    }

    std::println("Pipeline finished. Cleaning up...");

    hydralb::dpdk::Eal::cleanup();

    return 0;
}
