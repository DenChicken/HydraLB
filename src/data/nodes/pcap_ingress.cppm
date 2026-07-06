export module hydralb.data:pcap_ingress;

import :pipeline_node;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

class PcapIngressNode {
public:
    PcapIngressNode(const std::string& filename) : filename_(filename) {}

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        // TODO: read pcap
        return packets.subspan(0, 0);
    }

private:
    std::string filename_;
};

}  // namespace hydralb::data
