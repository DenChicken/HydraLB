export module hydralb.common.config;

import hydralb.common.network;
import std;

export namespace hydralb::config {

constexpr std::size_t MAGLEV_TABLE_SIZE = 65537;
constexpr std::size_t MAX_BACKENDS = 256;

constexpr std::size_t BURST_SIZE = 32;

constexpr std::int32_t ANY_SOCKET_ID = -1;

constexpr const char* VDEV_BUS_NAME = "vdev";

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
    std::uint32_t num_elements = 0;
    std::uint32_t cache_size = 0;
    std::int32_t socket_id = ANY_SOCKET_ID;
};

struct PcapIngressConfig {
    std::string device_name;
    std::string filename;
    std::string mempool_name;
};

struct PcapEgressConfig {
    std::string device_name;
    std::string filename;
};

struct Nodes {
    PcapIngressConfig pcap_ingress;
    PcapEgressConfig pcap_egress;
};

struct Environment {
    std::vector<std::string> eal_args;
};

struct Memory {
    std::vector<MempoolConfig> mempools;
};

struct Threading {
    std::vector<std::uint32_t> worker_lcores;
};

struct Balancing {
    std::vector<Backend> backends;
    network::IPv4Address local_tunnel_ip;
    std::uint32_t flow_hash_seed = 0;
};

struct AppConfig {
    PipelineMode mode = PipelineMode::PcapPassthrough;
    Environment environment;
    Memory memory;
    Threading threading;
    Balancing balancing;
    Nodes nodes;
};

}  // namespace hydralb::config
