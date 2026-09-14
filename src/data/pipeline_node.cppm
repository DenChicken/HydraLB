export module hydralb.data:pipeline_node;

import std;
import hydralb.dpdk;

export namespace hydralb::data {

template <typename T>
concept PipelineNode = requires(T node, const T const_node, std::span<dpdk::Packet> packets) {
    { node.configure() } -> std::same_as<std::expected<void, std::string>>;
    { node.process(packets) } -> std::same_as<std::span<dpdk::Packet>>;
    { node.shutdown() } -> std::same_as<void>;
    { const_node.dump_stats() } -> std::same_as<void>;
};

}  // namespace hydralb::data
