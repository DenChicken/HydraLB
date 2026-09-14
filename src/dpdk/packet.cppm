module;

#include <rte_mbuf.h>

export module hydralb.dpdk:packet;

import std;

export namespace hydralb::dpdk {

class alignas(8) Packet {
public:
    Packet() = default;
    Packet(::rte_mbuf* m) : mbuf_(m) {}

public:
    ::rte_mbuf* mbuf() const {
        return mbuf_;
    }

    template <typename T>
    T* data_at(std::uint32_t offset = 0) const {
        return mbuf_ ? rte_pktmbuf_mtod_offset(mbuf_, T*, offset) : nullptr;
    }

    std::uint8_t* prepend_headroom(std::uint16_t size) {
        return mbuf_ ? reinterpret_cast<std::uint8_t*>(::rte_pktmbuf_prepend(mbuf_, size))
                     : nullptr;
    }

    void free() {
        if (mbuf_) {
            ::rte_pktmbuf_free(mbuf_);
            mbuf_ = nullptr;
        }
    }

private:
    ::rte_mbuf* mbuf_ = nullptr;
};

static_assert(
    sizeof(Packet) == sizeof(::rte_mbuf*),
    "Packet must be a transparent wrapper over rte_mbuf*");

static_assert(alignof(Packet) == alignof(::rte_mbuf*), "Packet alignment must match rte_mbuf*");

}  // namespace hydralb::dpdk
