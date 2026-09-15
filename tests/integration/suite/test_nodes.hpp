#pragma once

import hydralb.dpdk;
import std;

namespace hydralb::test {

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

    void dump_stats() const {}

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

    void dump_stats() const {}

private:
    std::vector<dpdk::Packet>* collected_ = nullptr;
};

}  // namespace hydralb::test
