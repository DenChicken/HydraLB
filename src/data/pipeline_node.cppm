export module hydralb.data:pipeline_node;

import std;
import hydralb.dpdk;

export namespace hydralb::data {

template <typename T>
concept PipelineNode = requires(T node, std::span<dpdk::Packet> packets) {
    { node.process(packets) } -> std::same_as<std::span<dpdk::Packet>>;
};

}  // namespace hydralb::data
