module;

#include <rte_mbuf.h>

export module hydralb.dpdk:packet;

import std;

export namespace hydralb::dpdk {

struct alignas(8) Packet {
    Packet() = default;
    Packet(::rte_mbuf* m) : mbuf_(m) {}

    bool is_valid() const {
        return mbuf_ != nullptr;
    }

    ::rte_mbuf* mbuf() const {
        return mbuf_;
    }

    template <typename T>
    T* data_at(const uint32_t& offset = 0) {
        return reinterpret_cast<T*>(static_cast<char*>(mbuf_->buf_addr) + mbuf_->data_off + offset);
    }

    bool prepend_headroom(const uint16_t size) {
        return ::rte_pktmbuf_prepend(mbuf_, size) != nullptr;
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

}  // namespace hydralb::dpdk
