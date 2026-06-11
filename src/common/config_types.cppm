export module hydralb.common.config;

import hydralb.common.network;
import std;

export namespace hydralb::config {

constexpr std::size_t MAX_BACKENDS = 256;
constexpr std::size_t MAGLEV_TABLE_SIZE = 65537;

enum class BackendStatus : std::uint8_t {
    Dead,
    Alive,
};

struct Backend {
    std::uint32_t id = 0;
    network::IPv4Address ip;
    std::uint16_t port = 0;
    std::uint16_t weight = 1;
    BackendStatus status = BackendStatus::Dead;
};

struct RoutingTable {
    std::array<Backend, MAX_BACKENDS> backends{};
    std::size_t backend_count = 0;
    std::array<std::uint32_t, MAGLEV_TABLE_SIZE> lookup_table{};
    bool is_ready = false;
};

}  // namespace hydralb::config
