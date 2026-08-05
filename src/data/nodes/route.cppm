module;

#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_net.h>

export module hydralb.data:route;

import hydralb.dpdk;
import hydralb.common.config;
import hydralb.common.network;
import hydralb.algorithms.maglev;
import std;

export namespace hydralb::data {

class RouteNode {
public:
    explicit RouteNode(const config::RoutingTable* routing_table) : routing_table_(routing_table) {}

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
        std::ignore = pkt;
        // TODO
        return true;
    }

    const config::RoutingTable* routing_table_ = nullptr;
};

}  // namespace hydralb::data
