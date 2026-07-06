module;

#include <rte_ether.h>

export module hydralb.data:l2_reflector;

import :pipeline_node;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

class L2ReflectorNode {
public:
    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        for (auto& packet : packets) {
            if (!packet.is_valid()) {
                continue;
            }

            auto* eth_hdr = packet.data_at<struct ::rte_ether_hdr>(0);
            if (eth_hdr) {
                struct ::rte_ether_addr tmp = eth_hdr->dst_addr;
                eth_hdr->dst_addr = eth_hdr->src_addr;
                eth_hdr->src_addr = tmp;
            }
        }

        return packets;
    }
};

}  // namespace hydralb::data
