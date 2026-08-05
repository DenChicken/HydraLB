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
        std::ignore = pkt;
        // TODO
        return true;
    }

    const config::RoutingTable* routing_table_ = nullptr;
    std::uint32_t local_ip_ = 0;
};

}  // namespace hydralb::data
