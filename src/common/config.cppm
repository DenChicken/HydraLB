export module hydralb.common.config;

import hydralb.common.network;
import std;

export namespace hydralb::config {

constexpr std::size_t MAGLEV_TABLE_SIZE = 65537;
constexpr std::size_t MAX_BACKENDS = 256;

constexpr std::size_t BURST_SIZE = 32;

constexpr std::int32_t ANY_SOCKET_ID = -1;

constexpr const char* VDEV_BUS_NAME = "vdev";

constexpr std::string_view PCAP_RX_ARG_KEY = "rx_pcap";
constexpr std::string_view PCAP_TX_ARG_KEY = "tx_pcap";

enum class BackendStatus : std::uint8_t { Dead, Alive };

enum class PipelineMode : std::uint8_t { PcapPassthrough, PcapLoadBalancer };

struct Backend {
    std::uint32_t id = 0;
    network::IPv4Address ip;
    BackendStatus status = BackendStatus::Dead;
};

struct alignas(64) RoutingTable {
    std::array<std::uint32_t, MAGLEV_TABLE_SIZE> lookup_table{};
    std::array<Backend, MAX_BACKENDS> backends{};
    std::size_t backend_count = 0;
    std::atomic<bool> is_ready{false};
};

struct MempoolConfig {
    std::string name;
    std::uint32_t elements = 0;
    std::uint32_t cache = 0;
    std::int32_t socket_id = ANY_SOCKET_ID;
};

struct DeviceConfig {
    std::string name;
    std::string args;
};

struct QueueBinding {
    std::string device;
    std::uint16_t queue = 0;
};

struct WorkerConfig {
    std::uint32_t lcore = 0;
    PipelineMode mode = PipelineMode::PcapPassthrough;
    std::string mempool;
    QueueBinding rx;
    QueueBinding tx;
};

struct StartupConfig {
    std::vector<std::string> eal_args;
    std::vector<MempoolConfig> mempools;
    std::vector<DeviceConfig> devices;
    std::vector<WorkerConfig> workers;
};

struct RuntimeConfig {
    std::vector<Backend> backends;
    network::IPv4Address local_tunnel_ip;
    std::uint32_t flow_hash_seed = 0;
};

}  // namespace hydralb::config
