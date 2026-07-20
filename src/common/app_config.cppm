export module hydralb.common.app_config;

import std;

export namespace hydralb::config {

enum class PipelineMode : std::uint8_t {
    PcapPassthrough,
};

struct MempoolConfig {
    std::string name;
    std::uint32_t num_elements = 0;
    std::uint32_t cache_size = 0;
    std::int32_t socket_id = -1;
};

struct PcapIngressConfig {
    std::string filename;
    std::string mempool_name;
};

struct PcapEgressConfig {
    std::string filename;
};

struct NodeRegistry {
    PcapIngressConfig pcap_ingress;
    PcapEgressConfig pcap_egress;
};

struct Profile {
    PipelineMode mode = PipelineMode::PcapPassthrough;
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

struct AppConfig {
    Profile profile;
    Environment environment;
    Memory memory;
    Threading threading;
    NodeRegistry nodes;
};

}  // namespace hydralb::config
