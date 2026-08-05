module;

#include <rte_mbuf.h>
#include <rte_net.h>

export module hydralb.data:parser;

import hydralb.dpdk;
import std;

export namespace hydralb::data {

class ParserNode {
public:
    std::expected<void, std::string> configure() {
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t valid_count = 0;
        for (auto& pkt : packets) {
            if (is_valid(pkt)) {
                packets[valid_count++] = pkt;
            } else {
                pkt.free();
            }
        }
        return packets.subspan(0, valid_count);
    }

private:
    bool is_valid(const dpdk::Packet& pkt) const {
        std::ignore = pkt;
        // TODO
        return true;
    }
};

}  // namespace hydralb::data
