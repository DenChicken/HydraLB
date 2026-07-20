export module hydralb.data:bootstrap;

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

}  // namespace hydralb::data
