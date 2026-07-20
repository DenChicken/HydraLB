module;

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

export module hydralb.data:encap;

import hydralb.dpdk;
import hydralb.common.config;
import hydralb.common.network;
import std;

export namespace hydralb::data {

class EncapNode {
    static constexpr std::uint16_t ENCAP_HEADER_SIZE = sizeof(::rte_ipv4_hdr);
    static constexpr std::uint8_t IPV4_VERSION_IHL = 0x45;
    static constexpr std::uint8_t IP_IN_IP_PROTO = 4;
    static constexpr std::uint8_t DEFAULT_TTL = 64;
    static constexpr std::uint16_t DEFAULT_PACKET_ID = 0;
    static constexpr std::uint16_t DEFAULT_FRAGMENT_OFFSET = 0;

public:
    explicit EncapNode(const config::RoutingTable* routing_table, std::uint32_t local_ip)
        : routing_table_(routing_table), local_ip_(local_ip) {}

    std::expected<void, std::string> configure() {
        if (!routing_table_) {
            return std::unexpected("Routing table is null");
        }
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t valid_count = 0;
        for (auto& pkt : packets) {
            if (encapsulate(pkt)) {
                packets[valid_count++] = pkt;
            } else {
                pkt.free();
            }
        }
        return packets.subspan(0, valid_count);
    }

private:
    bool encapsulate(dpdk::Packet& pkt) const {
        auto* mbuf = pkt.mbuf();
        std::uint32_t backend_id = pkt.get_backend_id();

        const config::Backend* target_backend = nullptr;
        for (std::size_t i = 0; i < routing_table_->backend_count; ++i) {
            if (routing_table_->backends[i].id == backend_id) {
                target_backend = &routing_table_->backends[i];
                break;
            }
        }

        if (!target_backend || target_backend->status == config::BackendStatus::Dead) {
            return false;
        }

        if (!pkt.prepend_headroom(ENCAP_HEADER_SIZE)) {
            return false;
        }

        auto* inner_ip = pkt.data_at<::rte_ipv4_hdr>(ENCAP_HEADER_SIZE);
        if (!inner_ip) {
            return false;
        }

        auto* outer_ip = pkt.data_at<::rte_ipv4_hdr>(0);
        if (!outer_ip) {
            return false;
        }

        outer_ip->version_ihl = IPV4_VERSION_IHL;
        outer_ip->type_of_service = inner_ip->type_of_service;
        outer_ip->total_length = rte_cpu_to_be_16(mbuf->pkt_len);
        outer_ip->packet_id = DEFAULT_PACKET_ID;
        outer_ip->fragment_offset = DEFAULT_FRAGMENT_OFFSET;
        outer_ip->time_to_live = DEFAULT_TTL;
        outer_ip->next_proto_id = IP_IN_IP_PROTO;
        outer_ip->src_addr = rte_cpu_to_be_32(local_ip_);
        outer_ip->dst_addr = rte_cpu_to_be_32(target_backend->ip.address);
        outer_ip->hdr_checksum = 0;

        mbuf->l3_len = ENCAP_HEADER_SIZE;
        mbuf->ol_flags |= RTE_MBUF_F_TX_OUTER_IP_CKSUM | RTE_MBUF_F_TX_OUTER_IPV4;

        return true;
    }

    const config::RoutingTable* routing_table_ = nullptr;
    std::uint32_t local_ip_ = 0;
};

}  // namespace hydralb::data
