#pragma once

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>

import hydralb.common.network;
import hydralb.dpdk;
import std;

namespace hydralb::test {

constexpr std::uint8_t IPV4_VERSION_IHL = 0x45;
constexpr std::uint8_t IPV4_TTL = 64;
constexpr std::size_t ARP_BODY_SIZE = 28;
constexpr std::size_t PAYLOAD_SIZE = 64;
constexpr std::uint8_t PAYLOAD_FILL_BASE = 0x40;

struct FlowSpec {
    std::uint32_t src_ip = 0;
    std::uint32_t dst_ip = 0;
    std::uint16_t src_port = 0;
    std::uint16_t dst_port = 0;
    network::TransportProtocol protocol = network::TransportProtocol::TCP;
};

inline std::uint8_t* emplace_ether(dpdk::Packet& packet, std::uint16_t ether_type) {
    auto* ether = packet.data_at<::rte_ether_hdr>();
    *ether = ::rte_ether_hdr{};
    ether->ether_type = rte_cpu_to_be_16(ether_type);
    return reinterpret_cast<std::uint8_t*>(ether) + sizeof(::rte_ether_hdr);
}

inline void fill_payload(std::uint8_t* payload, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<std::uint8_t>(PAYLOAD_FILL_BASE + i);
    }
}

inline std::size_t l4_header_size(network::TransportProtocol protocol) {
    return protocol == network::TransportProtocol::UDP ? sizeof(::rte_udp_hdr)
                                                       : sizeof(::rte_tcp_hdr);
}

inline dpdk::Packet make_ipv4_packet(::rte_mempool* pool, const FlowSpec& flow) {
    const std::size_t l4_size = l4_header_size(flow.protocol);
    const std::size_t total =
        sizeof(::rte_ether_hdr) + sizeof(::rte_ipv4_hdr) + l4_size + PAYLOAD_SIZE;

    ::rte_mbuf* mbuf = ::rte_pktmbuf_alloc(pool);
    if (!mbuf || !::rte_pktmbuf_append(mbuf, static_cast<std::uint16_t>(total))) {
        return dpdk::Packet{};
    }

    dpdk::Packet packet{mbuf};
    emplace_ether(packet, RTE_ETHER_TYPE_IPV4);

    auto* ip = packet.data_at<::rte_ipv4_hdr>(sizeof(::rte_ether_hdr));
    *ip = ::rte_ipv4_hdr{};
    ip->version_ihl = IPV4_VERSION_IHL;
    ip->time_to_live = IPV4_TTL;
    ip->next_proto_id = static_cast<std::uint8_t>(flow.protocol);
    ip->src_addr = rte_cpu_to_be_32(flow.src_ip);
    ip->dst_addr = rte_cpu_to_be_32(flow.dst_ip);
    ip->total_length = rte_cpu_to_be_16(
        static_cast<std::uint16_t>(sizeof(::rte_ipv4_hdr) + l4_size + PAYLOAD_SIZE));
    ip->hdr_checksum = ::rte_ipv4_cksum(ip);

    const std::uint32_t l4_offset = sizeof(::rte_ether_hdr) + sizeof(::rte_ipv4_hdr);

    if (flow.protocol == network::TransportProtocol::UDP) {
        auto* udp = packet.data_at<::rte_udp_hdr>(l4_offset);
        *udp = ::rte_udp_hdr{};
        udp->src_port = rte_cpu_to_be_16(flow.src_port);
        udp->dst_port = rte_cpu_to_be_16(flow.dst_port);
        udp->dgram_len = rte_cpu_to_be_16(static_cast<std::uint16_t>(l4_size + PAYLOAD_SIZE));
    } else {
        auto* tcp = packet.data_at<::rte_tcp_hdr>(l4_offset);
        *tcp = ::rte_tcp_hdr{};
        tcp->src_port = rte_cpu_to_be_16(flow.src_port);
        tcp->dst_port = rte_cpu_to_be_16(flow.dst_port);
        tcp->data_off =
            static_cast<std::uint8_t>(sizeof(::rte_tcp_hdr) / sizeof(std::uint32_t) << 4);
    }

    fill_payload(
        packet.data_at<std::uint8_t>(static_cast<std::uint32_t>(l4_offset + l4_size)),
        PAYLOAD_SIZE);

    return packet;
}

inline dpdk::Packet make_arp_packet(::rte_mempool* pool) {
    const std::size_t total = sizeof(::rte_ether_hdr) + ARP_BODY_SIZE;

    ::rte_mbuf* mbuf = ::rte_pktmbuf_alloc(pool);
    if (!mbuf || !::rte_pktmbuf_append(mbuf, static_cast<std::uint16_t>(total))) {
        return dpdk::Packet{};
    }

    dpdk::Packet packet{mbuf};
    std::fill_n(emplace_ether(packet, RTE_ETHER_TYPE_ARP), ARP_BODY_SIZE, std::uint8_t{0});

    return packet;
}

inline std::vector<std::uint8_t> copy_bytes(const dpdk::Packet& packet) {
    const auto* start = packet.data_at<std::uint8_t>();
    return std::vector<std::uint8_t>(start, start + packet.mbuf()->pkt_len);
}

}  // namespace hydralb::test
