export module hydralb.data:bootstrap;

import hydralb.algorithms.maglev;
import hydralb.common.config;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

std::expected<void, std::string> setup_memory(const config::Memory& memory) {
    for (const auto& pool_config : memory.mempools) {
        auto pool_res = dpdk::Mempool::create(
            pool_config.name,
            pool_config.num_elements,
            pool_config.cache_size,
            pool_config.socket_id);
        if (!pool_res) {
            return std::unexpected(pool_res.error());
        }
    }

    return {};
}

std::expected<void, std::string> setup_routing(
    const config::Balancing& balancing,
    config::RoutingTable& routing_table) {
    if (balancing.backends.empty()) {
        return std::unexpected("No backends configured");
    }
    if (balancing.backends.size() > config::MAX_BACKENDS) {
        return std::unexpected(
            std::format(
                "Backend count {} exceeds maximum {}",
                balancing.backends.size(),
                config::MAX_BACKENDS));
    }

    std::ranges::copy(balancing.backends, routing_table.backends.begin());
    routing_table.backend_count = balancing.backends.size();

    algorithms::MaglevHasher::populate_table(
        routing_table.backends,
        routing_table.backend_count,
        routing_table.lookup_table);

    routing_table.is_ready.store(true, std::memory_order_release);

    return {};
}

}  // namespace hydralb::data
