module;

#include <rte_ether.h>

export module hydralb.data:l2_reflector;

import :pipeline_node;
import hydralb.common.app_config;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

class L2ReflectorNode {
public:
    L2ReflectorNode(const config::L2ReflectorConfig& config) : enabled_(config.enabled) {}

    std::expected<void, std::string> configure() {
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        if (!enabled_) {
            return packets;
        }

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

private:
    bool enabled_ = true;
};

}  // namespace hydralb::data
