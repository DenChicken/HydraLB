module;

#include <rte_byteorder.h>
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

struct ParserStage {
    static std::expected<network::FlowKey, std::string> parse(const dpdk::Packet& pkt) {
        struct rte_net_hdr_lens hdr_lens;
        std::uint32_t ptype = rte_net_get_ptype(pkt.mbuf(), &hdr_lens, RTE_PTYPE_ALL_MASK);

        if (!(ptype & RTE_PTYPE_L3_IPV4)) {
            return std::unexpected("Not IPv4");
        }

        auto* ip_hdr = pkt.data_at<rte_ipv4_hdr>(hdr_lens.l2_len);
        if (!ip_hdr) {
            return std::unexpected("Invalid IP header");
        }

        if ((ip_hdr->version_ihl & 0x0F) < 5) {
            return std::unexpected("IP header too short");
        }

        network::FlowKey key{};
        key.src_ip.address = ip_hdr->src_addr;
        key.dst_ip.address = ip_hdr->dst_addr;

        if (ptype & RTE_PTYPE_L4_TCP) {
            auto* tcp = pkt.data_at<rte_tcp_hdr>(hdr_lens.l2_len + hdr_lens.l3_len);
            if (!tcp) {
                return std::unexpected("Invalid TCP header");
            }
            key.protocol = network::TransportProtocol::TCP;
            key.src_port = rte_be_to_cpu_16(tcp->src_port);
            key.dst_port = rte_be_to_cpu_16(tcp->dst_port);
        } else if (ptype & RTE_PTYPE_L4_UDP) {
            auto* udp = pkt.data_at<rte_udp_hdr>(hdr_lens.l2_len + hdr_lens.l3_len);
            if (!udp) {
                return std::unexpected("Invalid UDP header");
            }
            key.protocol = network::TransportProtocol::UDP;
            key.src_port = rte_be_to_cpu_16(udp->src_port);
            key.dst_port = rte_be_to_cpu_16(udp->dst_port);
        } else {
            key.protocol = network::TransportProtocol::Unknown;
            return std::unexpected("Unsupported L4 protocol");
        }

        return key;
    }
};

struct RouteStage {
    const config::RoutingTable* routing_table = nullptr;

    std::expected<std::uint32_t, std::string> lookup(const network::FlowKey& key) const {
        if (!routing_table) {
            return std::unexpected("Routing table is null");
        }

        if (!routing_table->is_ready.load(std::memory_order_acquire)) {
            return std::unexpected("Routing table not ready");
        }

        std::uint32_t hash = rte_jhash(&key, sizeof(network::FlowKey), 0x12345678);
        std::uint32_t idx = hash % config::MAGLEV_TABLE_SIZE;
        std::uint32_t backend_id = routing_table->lookup_table[idx];

        if (backend_id == 0 || backend_id > config::MAX_BACKENDS) {
            return std::unexpected("Invalid backend ID");
        }

        const auto& backend = routing_table->backends[backend_id - 1];
        if (backend.status != config::BackendStatus::Alive) {
            return std::unexpected("Backend is dead");
        }

        return backend_id;
    }
};

struct EncapStage {
    const config::RoutingTable* routing_table = nullptr;
    std::uint32_t local_ip = 0;

    struct PrecomputedHeader {
        rte_ipv4_hdr outer_ip{};
        bool valid = false;
    };

    std::array<PrecomputedHeader, config::MAX_BACKENDS> precomputed_headers{};

    void rebuild_cache() {
        if (!routing_table) {
            return;
        }

        for (std::size_t i = 0; i < routing_table->backend_count && i < config::MAX_BACKENDS; ++i) {
            const auto& backend = routing_table->backends[i];
            if (backend.status != config::BackendStatus::Alive) {
                precomputed_headers[i].valid = false;
                continue;
            }

            auto& hdr = precomputed_headers[i];
            hdr.outer_ip.version_ihl = 0x45;
            hdr.outer_ip.type_of_service = 0;
            hdr.outer_ip.total_length = 0;
            hdr.outer_ip.packet_id = 0;
            hdr.outer_ip.fragment_offset = 0;
            hdr.outer_ip.time_to_live = 64;
            hdr.outer_ip.next_proto_id = IPPROTO_IPIP;
            hdr.outer_ip.src_addr = local_ip;
            hdr.outer_ip.dst_addr = backend.ip.address;
            hdr.outer_ip.hdr_checksum = 0;
            hdr.valid = true;
        }
    }

    std::expected<void, std::string> apply(dpdk::Packet& pkt, std::uint32_t backend_id) const {
        if (backend_id == 0 || backend_id > config::MAX_BACKENDS) {
            return std::unexpected("Invalid backend ID");
        }

        const auto& header = precomputed_headers[backend_id - 1];
        if (!header.valid) {
            return std::unexpected("Backend header not precomputed");
        }

        if (!pkt.prepend_headroom(sizeof(rte_ipv4_hdr))) {
            return std::unexpected("Failed to prepend headroom");
        }

        auto* outer_ip = pkt.data_at<rte_ipv4_hdr>();
        if (!outer_ip) {
            return std::unexpected("Failed to get outer IP header");
        }

        *outer_ip = header.outer_ip;
        outer_ip->total_length = rte_cpu_to_be_16(pkt.mbuf()->pkt_len);
        outer_ip->hdr_checksum = 0;
        outer_ip->hdr_checksum = rte_ipv4_cksum(outer_ip);

        pkt.mbuf()->packet_type = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_IP;

        return {};
    }
};

struct StatsStage {
    struct PerCoreStats {
        std::uint64_t packets_received = 0;
        std::uint64_t packets_dropped = 0;
        std::uint64_t packets_forwarded = 0;
        std::uint64_t parse_errors = 0;
        std::uint64_t route_errors = 0;
        std::uint64_t encap_errors = 0;
    };

    std::array<PerCoreStats, RTE_MAX_LCORE> stats{};
    std::uint32_t core_id = 0;

    void init(std::uint32_t lcore_id) {
        core_id = lcore_id;
        stats[core_id] = PerCoreStats{};
    }

    void record_received() {
        stats[core_id].packets_received++;
    }

    void record_dropped() {
        stats[core_id].packets_dropped++;
    }

    void record_forwarded() {
        stats[core_id].packets_forwarded++;
    }

    void record_parse_error() {
        stats[core_id].parse_errors++;
    }

    void record_route_error() {
        stats[core_id].route_errors++;
    }

    void record_encap_error() {
        stats[core_id].encap_errors++;
    }

    void dump_stats() const {
        const auto& s = stats[core_id];
        std::println("Core {} stats:", core_id);
        std::println("  Received:   {}", s.packets_received);
        std::println("  Forwarded:  {}", s.packets_forwarded);
        std::println("  Dropped:    {}", s.packets_dropped);
        std::println("  Parse err:  {}", s.parse_errors);
        std::println("  Route err:  {}", s.route_errors);
        std::println("  Encap err:  {}", s.encap_errors);
    }
};

}  // namespace hydralb::data

export namespace hydralb::data {

class LBNode {
public:
    struct Config {
        const config::RoutingTable* routing_table = nullptr;
        std::uint32_t local_tunnel_ip = 0x0A000001;
        bool enable_parser = true;
        bool enable_routing = true;
        bool enable_encap = true;
        bool enable_stats = true;
        std::uint32_t lcore_id = 0;
    };

    explicit LBNode(const Config& cfg)
        : config_(cfg),
          router_{.routing_table = cfg.routing_table},
          encap_{.routing_table = cfg.routing_table, .local_ip = cfg.local_tunnel_ip} {
        if (config_.enable_stats) {
            stats_.init(config_.lcore_id);
        }
        encap_.rebuild_cache();
    }

    std::expected<void, std::string> configure() {
        if (!config_.routing_table) {
            return std::unexpected("Routing table is null");
        }
        if (!config_.routing_table->is_ready.load(std::memory_order_acquire)) {
            return std::unexpected("Routing table not ready");
        }
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t valid_count = 0;

        for (auto& pkt : packets) {
            if (process_one(pkt)) {
                packets[valid_count++] = pkt;
            } else {
                pkt.free();
                if (config_.enable_stats) {
                    stats_.record_dropped();
                }
            }
        }

        return packets.subspan(0, valid_count);
    }

    void dump_stats() const {
        if (config_.enable_stats) {
            stats_.dump_stats();
        }
    }

private:
    bool process_one(dpdk::Packet& pkt) {
        if (config_.enable_stats) {
            stats_.record_received();
        }

        if (config_.enable_parser) {
            auto key_res = ParserStage::parse(pkt);
            if (!key_res) {
                if (config_.enable_stats) {
                    stats_.record_parse_error();
                }
                return false;
            }

            if (config_.enable_routing) {
                auto backend_res = router_.lookup(*key_res);
                if (!backend_res) {
                    if (config_.enable_stats) {
                        stats_.record_route_error();
                    }
                    return false;
                }

                pkt.set_backend_id(*backend_res);

                if (config_.enable_encap) {
                    auto encap_res = encap_.apply(pkt, *backend_res);
                    if (!encap_res) {
                        if (config_.enable_stats) {
                            stats_.record_encap_error();
                        }
                        return false;
                    }
                }
            }
        }

        if (config_.enable_stats) {
            stats_.record_forwarded();
        }
        return true;
    }

private:
    Config config_;
    RouteStage router_;
    EncapStage encap_;
    StatsStage stats_;
};

}  // namespace hydralb::data
