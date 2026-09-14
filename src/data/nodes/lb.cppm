module;

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_tcp.h>
#include <rte_udp.h>

export module hydralb.data:lb;

import :pipeline_node;
import hydralb.common.config;
import hydralb.common.network;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::uint8_t IPV4_IHL_MASK = 0x0F;
constexpr std::uint8_t IPV4_IHL_MIN = 5;
constexpr std::uint8_t IPV4_VERSION_IHL_DEFAULT = 0x45;
constexpr std::uint8_t IPV4_TTL_DEFAULT = 64;

}  // namespace hydralb::data

export namespace hydralb::data {

enum class DropReason : std::uint8_t {
    NotIpv4,
    MalformedIpv4,
    UnsupportedL4,
    RoutingTableNotReady,
    NoBackend,
    BackendDead,
    EncapNoHeadroom,
};

constexpr std::size_t DROP_REASON_COUNT = std::to_underlying(DropReason::EncapNoHeadroom) + 1;

constexpr std::string_view drop_reason_name(DropReason reason) {
    switch (reason) {
        case DropReason::NotIpv4:
            return "not_ipv4";
        case DropReason::MalformedIpv4:
            return "malformed_ipv4";
        case DropReason::UnsupportedL4:
            return "unsupported_l4";
        case DropReason::RoutingTableNotReady:
            return "routing_table_not_ready";
        case DropReason::NoBackend:
            return "no_backend";
        case DropReason::BackendDead:
            return "backend_dead";
        case DropReason::EncapNoHeadroom:
            return "encap_no_headroom";
        default:
            return "unknown";
    }
}

}  // namespace hydralb::data

namespace hydralb::data {

struct ParsedPacket {
    network::FlowKey key;
    std::uint16_t l2_len = 0;
};

struct ParserStage {
    static std::expected<ParsedPacket, DropReason> parse(const dpdk::Packet& pkt) {
        ::rte_net_hdr_lens hdr_lens{};
        const std::uint32_t ptype = ::rte_net_get_ptype(pkt.mbuf(), &hdr_lens, RTE_PTYPE_ALL_MASK);

        if (!(ptype & RTE_PTYPE_L3_IPV4)) {
            return std::unexpected(DropReason::NotIpv4);
        }

        const auto* ip_hdr = pkt.data_at<::rte_ipv4_hdr>(hdr_lens.l2_len);
        if (!ip_hdr || (ip_hdr->version_ihl & IPV4_IHL_MASK) < IPV4_IHL_MIN) {
            return std::unexpected(DropReason::MalformedIpv4);
        }

        ParsedPacket parsed{};
        parsed.l2_len = hdr_lens.l2_len;
        parsed.key.src_ip.address = ip_hdr->src_addr;
        parsed.key.dst_ip.address = ip_hdr->dst_addr;

        const std::uint32_t l4_offset = hdr_lens.l2_len + hdr_lens.l3_len;

        if (ptype & RTE_PTYPE_L4_TCP) {
            const auto* tcp = pkt.data_at<::rte_tcp_hdr>(l4_offset);
            if (!tcp) {
                return std::unexpected(DropReason::UnsupportedL4);
            }
            parsed.key.protocol = network::TransportProtocol::TCP;
            parsed.key.src_port = rte_be_to_cpu_16(tcp->src_port);
            parsed.key.dst_port = rte_be_to_cpu_16(tcp->dst_port);
            return parsed;
        }

        if (ptype & RTE_PTYPE_L4_UDP) {
            const auto* udp = pkt.data_at<::rte_udp_hdr>(l4_offset);
            if (!udp) {
                return std::unexpected(DropReason::UnsupportedL4);
            }
            parsed.key.protocol = network::TransportProtocol::UDP;
            parsed.key.src_port = rte_be_to_cpu_16(udp->src_port);
            parsed.key.dst_port = rte_be_to_cpu_16(udp->dst_port);
            return parsed;
        }

        return std::unexpected(DropReason::UnsupportedL4);
    }
};

struct RouteStage {
    const config::RoutingTable* routing_table = nullptr;
    std::uint32_t hash_seed = 0;

    std::expected<std::uint32_t, DropReason> lookup(const network::FlowKey& key) const {
        if (!routing_table->is_ready.load(std::memory_order_acquire)) {
            return std::unexpected(DropReason::RoutingTableNotReady);
        }

        const std::uint32_t hash = ::rte_jhash(&key, sizeof(key), hash_seed);
        const std::uint32_t backend_index =
            routing_table->lookup_table[hash % config::MAGLEV_TABLE_SIZE];

        if (backend_index >= routing_table->backend_count) {
            return std::unexpected(DropReason::NoBackend);
        }

        if (routing_table->backends[backend_index].status != config::BackendStatus::Alive) {
            return std::unexpected(DropReason::BackendDead);
        }

        return backend_index;
    }
};

struct EncapStage {
    const config::RoutingTable* routing_table = nullptr;
    std::uint32_t local_ip = 0;

    std::array<::rte_ipv4_hdr, config::MAX_BACKENDS> headers{};

    void rebuild_cache() {
        const std::size_t count = std::min(routing_table->backend_count, config::MAX_BACKENDS);

        for (std::size_t i = 0; i < count; ++i) {
            auto& hdr = headers[i];
            hdr.version_ihl = IPV4_VERSION_IHL_DEFAULT;
            hdr.time_to_live = IPV4_TTL_DEFAULT;
            hdr.next_proto_id = IPPROTO_IPIP;
            hdr.src_addr = rte_cpu_to_be_32(local_ip);
            hdr.dst_addr = rte_cpu_to_be_32(routing_table->backends[i].ip.address);
        }
    }

    std::expected<void, DropReason>
    apply(dpdk::Packet& pkt, std::uint32_t backend_index, std::uint16_t l2_len) const {
        auto* start = pkt.prepend_headroom(sizeof(::rte_ipv4_hdr));
        if (!start) {
            return std::unexpected(DropReason::EncapNoHeadroom);
        }

        std::memmove(start, start + sizeof(::rte_ipv4_hdr), l2_len);

        auto* outer_ip = pkt.data_at<::rte_ipv4_hdr>(l2_len);
        *outer_ip = headers[backend_index];
        outer_ip->total_length =
            rte_cpu_to_be_16(static_cast<std::uint16_t>(pkt.mbuf()->pkt_len - l2_len));
        outer_ip->hdr_checksum = ::rte_ipv4_cksum(outer_ip);

        pkt.mbuf()->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP;

        return {};
    }
};

struct Stats {
    std::uint64_t received = 0;
    std::uint64_t forwarded = 0;
    std::uint64_t dropped = 0;
    std::array<std::uint64_t, DROP_REASON_COUNT> drops{};

    void record_drop(DropReason reason) {
        ++dropped;
        ++drops[std::to_underlying(reason)];
    }
};

}  // namespace hydralb::data

export namespace hydralb::data {

class LBNode {
public:
    struct Config {
        const config::RoutingTable* routing_table = nullptr;
        std::uint32_t local_tunnel_ip = 0;
        std::uint32_t flow_hash_seed = 0;
        std::uint32_t lcore_id = 0;
    };

    explicit LBNode(const Config& config)
        : config_(config),
          router_{.routing_table = config.routing_table, .hash_seed = config.flow_hash_seed},
          encap_{.routing_table = config.routing_table, .local_ip = config.local_tunnel_ip} {}

    std::expected<void, std::string> configure() {
        if (!config_.routing_table) {
            return std::unexpected("Routing table is null");
        }
        if (!config_.routing_table->is_ready.load(std::memory_order_acquire)) {
            return std::unexpected("Routing table not ready");
        }

        encap_.rebuild_cache();

        return {};
    }

    void shutdown() {}

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t forwarded = 0;

        for (auto& pkt : packets) {
            ++stats_.received;

            auto result = process_one(pkt);
            if (result) {
                packets[forwarded++] = pkt;
                ++stats_.forwarded;
            } else {
                stats_.record_drop(result.error());
                pkt.free();
            }
        }

        return packets.subspan(0, forwarded);
    }

    void dump_stats() const {
        std::println("Core {} stats:", config_.lcore_id);
        std::println("  received:  {}", stats_.received);
        std::println("  forwarded: {}", stats_.forwarded);
        std::println("  dropped:   {}", stats_.dropped);

        for (std::size_t i = 0; i < DROP_REASON_COUNT; ++i) {
            if (stats_.drops[i] != 0) {
                std::println(
                    "    {}: {}",
                    drop_reason_name(static_cast<DropReason>(i)),
                    stats_.drops[i]);
            }
        }
    }

private:
    std::expected<void, DropReason> process_one(dpdk::Packet& pkt) {
        auto parsed = ParserStage::parse(pkt);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }

        auto backend_index = router_.lookup(parsed->key);
        if (!backend_index) {
            return std::unexpected(backend_index.error());
        }

        return encap_.apply(pkt, *backend_index, parsed->l2_len);
    }

private:
    Config config_;
    RouteStage router_;
    EncapStage encap_;
    Stats stats_;
};

}  // namespace hydralb::data
