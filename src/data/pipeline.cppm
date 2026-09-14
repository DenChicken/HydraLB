export module hydralb.data:pipeline;

import :pipeline_node;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

template <std::size_t MaxWorkers, PipelineNode... Nodes>
class Pipeline {
public:
    static constexpr std::size_t max_workers = MaxWorkers;

    constexpr Pipeline(Nodes&&... nodes) : nodes_(std::forward<Nodes>(nodes)...) {}

    constexpr std::expected<void, std::string> configure() {
        return configure_priv(std::make_index_sequence<sizeof...(Nodes)>{});
    }

    constexpr std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        return process_priv(packets, std::make_index_sequence<sizeof...(Nodes)>{});
    }

    void dump_stats() const {
        dump_stats_priv(std::make_index_sequence<sizeof...(Nodes)>{});
    }

private:
    template <std::size_t... Is>
    void dump_stats_priv(std::index_sequence<Is...>) const {
        (dump_node_stats(std::get<Is>(nodes_)), ...);
    }

    template <typename Node>
    static void dump_node_stats(const Node& node) {
        if constexpr (requires { node.dump_stats(); }) {
            node.dump_stats();
        }
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
