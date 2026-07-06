export module hydralb.data:pcap_egress;

import :pipeline_node;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

class PcapEgressNode {
public:
    PcapEgressNode(const std::string& filename) : filename_(filename) {}

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        for (auto& packet : packets) {
            if (packet.is_valid()) {
                // TODO: write pcap
                packet.free();
            }
        }

        return {};
    }

private:
    std::string filename_;
};

}  // namespace hydralb::data
