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
        struct ::rte_net_hdr_lens hdr_lens{};
        std::uint32_t ptype = ::rte_net_get_ptype(pkt.mbuf(), &hdr_lens, RTE_PTYPE_ALL_MASK);

        bool is_ipv4 = (ptype & RTE_PTYPE_L3_IPV4) != 0;
        bool is_tcp_or_udp = ((ptype & RTE_PTYPE_L4_TCP) != 0) || ((ptype & RTE_PTYPE_L4_UDP) != 0);

        return is_ipv4 && is_tcp_or_udp;
    }
};

}  // namespace hydralb::data
