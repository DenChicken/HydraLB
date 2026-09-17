#pragma once

import hydralb.data;
import hydralb.dpdk;
import std;

namespace hydralb::test {

constexpr std::string_view SOURCE_NODE_NAME = "source";
constexpr std::string_view SINK_NODE_NAME = "sink";

class SourceNode {
public:
    explicit SourceNode(std::span<const dpdk::Packet> packets) : packets_(packets) {}

    std::expected<void, std::string> configure() {
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> batch) {
        const std::size_t count = std::min(packets_.size() - cursor_, batch.size());

        std::copy_n(packets_.begin() + static_cast<std::ptrdiff_t>(cursor_), count, batch.begin());
        cursor_ += count;

        return batch.subspan(0, count);
    }

    void shutdown() {}

    data::NodeStats collect_stats() const {
        return data::NodeStats{.node = SOURCE_NODE_NAME, .counters = {}};
    }

private:
    std::span<const dpdk::Packet> packets_;
    std::size_t cursor_ = 0;
};

class SinkNode {
public:
    explicit SinkNode(std::vector<dpdk::Packet>& collected) : collected_(&collected) {}

    std::expected<void, std::string> configure() {
        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> batch) {
        collected_->insert(collected_->end(), batch.begin(), batch.end());
        return batch;
    }

    void shutdown() {}

    data::NodeStats collect_stats() const {
        return data::NodeStats{.node = SINK_NODE_NAME, .counters = {}};
    }

private:
    std::vector<dpdk::Packet>* collected_ = nullptr;
};

}  // namespace hydralb::test
