export module hydralb.data:pipeline_node;

import std;
import hydralb.dpdk;

export namespace hydralb::data {

constexpr std::string_view RECEIVED_COUNTER = "received";
constexpr std::string_view FORWARDED_COUNTER = "forwarded";
constexpr std::string_view SENT_COUNTER = "sent";
constexpr std::string_view DROPPED_COUNTER = "dropped";

struct Counter {
    std::string_view name;
    std::uint64_t value = 0;
};

struct NodeStats {
    std::string_view node;
    std::vector<Counter> counters;
};

template <typename T>
concept PipelineNode = requires(const T node, T mutable_node, std::span<dpdk::Packet> packets) {
    { mutable_node.configure() } -> std::same_as<std::expected<void, std::string>>;
    { mutable_node.process(packets) } -> std::same_as<std::span<dpdk::Packet>>;
    { mutable_node.shutdown() } -> std::same_as<void>;
    { node.collect_stats() } -> std::same_as<NodeStats>;
};

void print_stats(std::uint32_t lcore_id, std::span<const NodeStats> stats) {
    if (stats.empty()) {
        return;
    }

    std::println("Core {} stats:", lcore_id);

    for (const auto& node_stats : stats) {
        if (node_stats.counters.empty()) {
            continue;
        }

        std::println("  {}:", node_stats.node);
        for (const auto& counter : node_stats.counters) {
            std::println("    {}: {}", counter.name, counter.value);
        }
    }
}

}  // namespace hydralb::data
