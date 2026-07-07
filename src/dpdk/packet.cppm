module;

#include <rte_byteorder.h>
#include <rte_hash_crc.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_sctp.h>
#include <rte_tcp.h>
#include <rte_udp.h>

export module hydralb.dpdk:packet;

import std;

export namespace hydralb::dpdk {

class alignas(8) Packet {
public:
    Packet() = default;
    Packet(::rte_mbuf* m) : mbuf_(m) {}

public:
    bool is_valid() const {
        return mbuf_ != nullptr;
    }

    ::rte_mbuf* mbuf() const {
        return mbuf_;
    }

    template <typename T>
    T* data_at(std::uint32_t offset = 0) const {
        return rte_pktmbuf_mtod_offset(mbuf_, T*, offset);
    }

    bool prepend_headroom(std::uint16_t size) {
        return ::rte_pktmbuf_prepend(mbuf_, size) != nullptr;
    }

    void free() {
        if (mbuf_) {
            ::rte_pktmbuf_free(mbuf_);
            mbuf_ = nullptr;
        }
    }

public:
    void compute_sw_cksums() {
        struct rte_net_hdr_lens hdr_lens;
        std::uint32_t ptype = ::rte_net_get_ptype(mbuf_, &hdr_lens, RTE_PTYPE_ALL_MASK);

        if (!(ptype & RTE_PTYPE_L3_IPV4)) {
            return;
        }

        auto* ip_hdr = data_at<::rte_ipv4_hdr>(hdr_lens.l2_len);
        if (!ip_hdr) {
            return;
        }

        ip_hdr->hdr_checksum = 0;
        ip_hdr->hdr_checksum = ::rte_ipv4_cksum(ip_hdr);

        auto* l4_ptr = data_at<std::uint8_t>(hdr_lens.l2_len + hdr_lens.l3_len);
        if (!l4_ptr) {
            return;
        }
        void* l4_hdr = static_cast<void*>(l4_ptr);

        if (ptype & RTE_PTYPE_L4_UDP) {
            auto* udp_hdr = reinterpret_cast<struct ::rte_udp_hdr*>(l4_hdr);
            std::uint16_t cksum = ::rte_ipv4_udptcp_cksum(ip_hdr, l4_hdr);
            udp_hdr->dgram_cksum = (cksum == 0) ? 0xFFFF : cksum;
        } else if (ptype & RTE_PTYPE_L4_TCP) {
            auto* tcp_hdr = reinterpret_cast<struct ::rte_tcp_hdr*>(l4_hdr);
            tcp_hdr->cksum = 0;
            tcp_hdr->cksum = ::rte_ipv4_udptcp_cksum(ip_hdr, l4_hdr);
        } else if (ptype & RTE_PTYPE_L4_SCTP) {
            auto* sctp_hdr = reinterpret_cast<struct ::rte_sctp_hdr*>(l4_hdr);
            sctp_hdr->cksum = 0;
            std::uint16_t ip_len = rte_be_to_cpu_16(ip_hdr->total_length);
            std::uint16_t sctp_len = ip_len - hdr_lens.l3_len;
            sctp_hdr->cksum = rte_cpu_to_be_32(::rte_hash_crc(sctp_hdr, sctp_len, 0xFFFFFFFF));
        }
    }

    void prepare_hw_cksums() {
        struct rte_net_hdr_lens hdr_lens;
        std::uint32_t ptype = ::rte_net_get_ptype(mbuf_, &hdr_lens, RTE_PTYPE_ALL_MASK);

        if (!(ptype & RTE_PTYPE_L3_IPV4)) {
            return;
        }

        mbuf_->l2_len = hdr_lens.l2_len;
        mbuf_->l3_len = hdr_lens.l3_len;
        mbuf_->ol_flags |= RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_IPV4;

        auto* ip_hdr = data_at<::rte_ipv4_hdr>(hdr_lens.l2_len);
        auto* l4_ptr = data_at<std::uint8_t>(hdr_lens.l2_len + hdr_lens.l3_len);
        void* l4_hdr = l4_ptr ? static_cast<void*>(l4_ptr) : nullptr;

        if (!ip_hdr || !l4_hdr) {
            return;
        }

        if (ptype & RTE_PTYPE_L4_UDP) {
            auto* udp_hdr = reinterpret_cast<struct ::rte_udp_hdr*>(l4_hdr);
            mbuf_->ol_flags |= RTE_MBUF_F_TX_UDP_CKSUM;
            udp_hdr->dgram_cksum = ::rte_ipv4_phdr_cksum(ip_hdr, mbuf_->ol_flags);
        } else if (ptype & RTE_PTYPE_L4_TCP) {
            auto* tcp_hdr = reinterpret_cast<struct ::rte_tcp_hdr*>(l4_hdr);
            mbuf_->ol_flags |= RTE_MBUF_F_TX_TCP_CKSUM;
            tcp_hdr->cksum = ::rte_ipv4_phdr_cksum(ip_hdr, mbuf_->ol_flags);
        } else if (ptype & RTE_PTYPE_L4_SCTP) {
            auto* sctp_hdr = data_at<::rte_sctp_hdr>(hdr_lens.l2_len + hdr_lens.l3_len);
            if (sctp_hdr) {
                mbuf_->ol_flags |= RTE_MBUF_F_TX_SCTP_CKSUM;
                sctp_hdr->cksum = 0;
            }
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
