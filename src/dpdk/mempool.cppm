module;

#include <rte_mbuf.h>
#include <rte_mempool.h>

export module hydralb.dpdk:mempool;

import std;

export namespace hydralb::dpdk {

class Mempool {
public:
    Mempool() = default;

    Mempool(::rte_mempool* pool) : pool_(pool) {}

public:
    bool is_valid() const {
        return pool_ != nullptr;
    }

    ::rte_mempool* const raw() {
        return pool_;
    }

public:
    static std::expected<Mempool, std::string> create(
        const std::string& name,
        std::uint32_t num_elements,
        std::uint32_t cache_size,
        std::uint16_t data_room_size = RTE_MBUF_DEFAULT_BUF_SIZE) {
        ::rte_mempool* pool = ::rte_pktmbuf_pool_create(
            name.data(),
            num_elements,
            cache_size,
            0,
            data_room_size,
            SOCKET_ID_ANY);

        if (!pool) {
            return std::unexpected(std::format("Failed to create mempool: {}", name));
        }

        return Mempool(pool);
    }

    static std::optional<Mempool> find_by_name(const std::string& name){
        ::rte_mempool* pool = ::rte_mempool_lookup(name.c_str());
        if (!pool) {
            return std::nullopt;
        }
        return Mempool(pool);
    }

private:
    ::rte_mempool* pool_ = nullptr;
};

}  // namespace hydralb::dpdk
