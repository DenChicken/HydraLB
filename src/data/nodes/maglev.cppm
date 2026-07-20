module;

#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_net.h>

export module hydralb.data:maglev;

import hydralb.dpdk;
import hydralb.common.config;
import hydralb.common.network;
import hydralb.algorithms.maglev;
import std;

export namespace hydralb::data {

class MaglevNode {
    static constexpr std::uint32_t JHASH_INIT_VAL = 0;

public:
    explicit MaglevNode(const config::RoutingTable* routing_table)
        : routing_table_(routing_table) {}

    std::expected<void, std::string> configure() {
        if (!routing_table_) {
            return std::unexpected("Routing table is null");
        }
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t valid_count = 0;
        for (auto& pkt : packets) {
            if (route(pkt)) {
                packets[valid_count++] = pkt;
            } else {
                pkt.free();
            }
        }
        return packets.subspan(0, valid_count);
    }

private:
    bool route(dpdk::Packet& pkt) const {
        if (!routing_table_->is_ready) {
            return false;
        }

        auto* mbuf = pkt.mbuf();
        std::uint32_t hash = 0;

        if ((mbuf->ol_flags & RTE_MBUF_F_RX_RSS_HASH) != 0) {
            hash = mbuf->hash.rss;
        } else {
            struct ::rte_net_hdr_lens hdr_lens{};
            std::uint32_t ptype =
                ::rte_net_get_ptype(mbuf, &hdr_lens, RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK);

            auto* ipv4_hdr = pkt.data_at<::rte_ipv4_hdr>(hdr_lens.l2_len);
            if (!ipv4_hdr) {
                return false;
            }

            std::uint32_t ports = 0;
            if (ptype & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP)) {
                auto* l4_ports = pkt.data_at<std::uint32_t>(hdr_lens.l2_len + hdr_lens.l3_len);
                if (l4_ports) {
                    ports = *l4_ports;
                }
            }

            hash =
                ::rte_jhash_3words(ipv4_hdr->src_addr, ipv4_hdr->dst_addr, ports, JHASH_INIT_VAL);
        }

        std::uint32_t backend_id = routing_table_->lookup_table[hash % config::MAGLEV_TABLE_SIZE];
        if (backend_id == algorithms::LOOKUP_INVALID_ID) {
            return false;
        }

        pkt.set_backend_id(backend_id);
        return true;
    }

    const config::RoutingTable* routing_table_ = nullptr;
};

}  // namespace hydralb::data
