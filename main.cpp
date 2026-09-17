import std;
import hydralb.dpdk;
import hydralb.data;
import hydralb.common.config;

namespace {

using namespace hydralb::config;

constexpr std::uint32_t MEMPOOL_ELEMENTS = 2047;
constexpr std::uint32_t MEMPOOL_CACHE = 256;

constexpr std::uint32_t LOCAL_TUNNEL_IP = 0x0A000001;
constexpr std::uint32_t FLOW_HASH_SEED = 0x5BD1E995;

constexpr std::uint32_t BACKEND_IP_BASE = 0x0A00000A;
constexpr std::size_t BACKEND_COUNT = 4;

constexpr std::uint32_t FIRST_WORKER_LCORE = 1;

constexpr std::string_view MEMPOOL_NAME_FORMAT = "HYDRALB_POOL_{}";
constexpr std::string_view RX_DEVICE_NAME_FORMAT = "net_pcap_ingress_{}";
constexpr std::string_view TX_DEVICE_NAME_FORMAT = "net_pcap_egress_{}";
constexpr std::string_view OUTPUT_PCAP_FORMAT = "output-{}.pcap";

const std::array INPUT_PCAPS = {
    std::string_view("pcap/https-and-dns-to-google.com.pcapng"),
    std::string_view("pcap/tcp-ecn-sample.pcap")};

StartupConfig make_startup_config() {
    StartupConfig startup;

    startup.eal_args = {"HydraLB", "-c", "0xf", "-n", "4", "--no-huge"};

    for (std::size_t i = 0; i < INPUT_PCAPS.size(); ++i) {
        const std::string suffix = std::to_string(i);

        const auto named = [&suffix](std::string_view format) {
            return std::vformat(format, std::make_format_args(suffix));
        };

        const std::string mempool_name = named(MEMPOOL_NAME_FORMAT);
        const std::string rx_device = named(RX_DEVICE_NAME_FORMAT);
        const std::string tx_device = named(TX_DEVICE_NAME_FORMAT);

        startup.mempools.push_back(
            MempoolConfig{
                .name = mempool_name,
                .elements = MEMPOOL_ELEMENTS,
                .cache = MEMPOOL_CACHE,
                .socket_id = ANY_SOCKET_ID});

        startup.devices.push_back(
            DeviceConfig{
                .name = rx_device,
                .args = std::format("{}={}", PCAP_RX_ARG_KEY, INPUT_PCAPS[i])});

        startup.devices.push_back(
            DeviceConfig{
                .name = tx_device,
                .args = std::format("{}={}", PCAP_TX_ARG_KEY, named(OUTPUT_PCAP_FORMAT))});

        startup.workers.push_back(
            WorkerConfig{
                .lcore = FIRST_WORKER_LCORE + static_cast<std::uint32_t>(i),
                .mode = PipelineMode::PcapLoadBalancer,
                .mempool = mempool_name,
                .rx = {.device = rx_device},
                .tx = {.device = tx_device}});
    }

    return startup;
}

RuntimeConfig make_runtime_config() {
    RuntimeConfig runtime;

    runtime.local_tunnel_ip = {.address = LOCAL_TUNNEL_IP};
    runtime.flow_hash_seed = FLOW_HASH_SEED;

    for (std::size_t i = 0; i < BACKEND_COUNT; ++i) {
        runtime.backends.push_back(
            Backend{
                .id = static_cast<std::uint32_t>(i + 1),
                .ip = {.address = BACKEND_IP_BASE + static_cast<std::uint32_t>(i)},
                .status = BackendStatus::Alive});
    }

    return runtime;
}

std::expected<void, std::string> run_pipelines(
    const StartupConfig& startup,
    const RuntimeConfig& runtime) {
    auto signals_ok = hydralb::dpdk::Signals::install_handlers();
    if (!signals_ok) {
        return signals_ok;
    }

    auto memory_ok = hydralb::data::setup_memory(startup);
    if (!memory_ok) {
        return memory_ok;
    }

    auto devices_ok = hydralb::data::setup_devices(startup);
    if (!devices_ok) {
        return devices_ok;
    }

    RoutingTable routing_table;

    auto routing_ok = hydralb::data::setup_routing(runtime, routing_table);
    if (!routing_ok) {
        hydralb::data::teardown_devices(startup);
        return routing_ok;
    }

    std::println("DPDK EAL, mempools and routing table are ready");

    auto run_ok = hydralb::data::Dispatcher::run(startup, runtime, routing_table);

    hydralb::data::teardown_devices(startup);

    return run_ok;
}

}  // namespace

int main() {
    const StartupConfig startup = make_startup_config();
    const RuntimeConfig runtime = make_runtime_config();

    auto valid_ok = hydralb::data::validate_startup(startup);
    if (!valid_ok) {
        std::println(std::cerr, "Error: {}", valid_ok.error());
        return 1;
    }

    auto init_ok = hydralb::dpdk::Eal::init(startup.eal_args);
    if (!init_ok) {
        std::println(std::cerr, "Error: {}", init_ok.error());
        return 1;
    }

    auto pipelines_ok = run_pipelines(startup, runtime);

    std::println("Pipeline finished. Cleaning up...");

    hydralb::dpdk::Eal::cleanup();

    if (!pipelines_ok) {
        std::println(std::cerr, "Error: {}", pipelines_ok.error());
        return 1;
    }

    return 0;
}
