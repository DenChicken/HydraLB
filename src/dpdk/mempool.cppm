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

    bool is_valid() const {
        return pool_ != nullptr;
    }

    ::rte_mempool* raw() {
        return pool_;
    }

    static std::expected<Mempool, std::string> create(
        const std::string_view& name,
        uint32_t num_elements,
        uint32_t cache_size,
        uint16_t data_room_size = RTE_MBUF_DEFAULT_BUF_SIZE) {
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

private:
    ::rte_mempool* pool_ = nullptr;
};

}  // namespace hydralb::dpdk
