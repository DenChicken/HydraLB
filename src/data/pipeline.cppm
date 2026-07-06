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
        return configure_priv(std::make_index_sequence<sizeof...(Nodes)>{});
    }

    constexpr std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        return process_priv(packets, std::make_index_sequence<sizeof...(Nodes)>{});
    }

private:
    template <std::size_t... Is>
    constexpr std::expected<void, std::string> configure_priv(std::index_sequence<Is...>) {
        std::expected<void, std::string> result;
        ((result = result ? std::get<Is>(nodes_).configure() : result), ...);
        return result;
    }

    template <std::size_t... Is>
    constexpr std::span<dpdk::Packet> process_priv(
        std::span<dpdk::Packet> packets,
        std::index_sequence<Is...>) {
        std::span<dpdk::Packet> current_batch = packets;
        ((current_batch = std::get<Is>(nodes_).process(current_batch)), ...);
        return current_batch;
    }

private:
    std::tuple<Nodes...> nodes_;
};

}  // namespace hydralb::data
