export module hydralb.data:pipeline;

import :pipeline_node;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

template <PipelineNode... Nodes>
class Pipeline {
public:
    constexpr Pipeline(Nodes&&... nodes) : nodes_(std::forward<Nodes>(nodes)...) {}

    constexpr std::expected<void, std::string> configure() {
        std::expected<void, std::string> result;

        std::apply(
            [&result](auto&... nodes) {
                ((result = result ? nodes.configure() : result), ...);
            },
            nodes_);

        return result;
    }

    constexpr std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::apply(
            [&packets](auto&... nodes) {
                ((packets = nodes.process(packets)), ...);
            },
            nodes_);

        return packets;
    }

    void shutdown() {
        std::apply(
            [](auto&... nodes) {
                (nodes.shutdown(), ...);
            },
            nodes_);
    }

    void dump_stats() const {
        std::apply(
            [](const auto&... nodes) {
                (nodes.dump_stats(), ...);
            },
            nodes_);
    }

private:
    std::tuple<Nodes...> nodes_;
};

}  // namespace hydralb::data
