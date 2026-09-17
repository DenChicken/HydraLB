module;

#include <rte_ethdev.h>

export module hydralb.data:pcap_egress;

import :pipeline_node;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::uint16_t PCAP_EGRESS_RX_QUEUES = 0;
constexpr std::uint16_t PCAP_EGRESS_TX_QUEUES = 1;

constexpr std::string_view PCAP_EGRESS_NODE_NAME = "pcap_egress";

}  // namespace hydralb::data

export namespace hydralb::data {

class PcapEgressNode {
public:
    struct Config {
        std::string device_name;
        std::uint16_t queue_id = 0;
    };

    explicit PcapEgressNode(const Config& config)
        : device_name_(config.device_name), queue_id_(config.queue_id) {}

    std::expected<void, std::string> configure() {
        auto port_res = dpdk::Device::find_by_name(device_name_);
        if (!port_res) {
            return std::unexpected(port_res.error());
        }

        device_ = dpdk::Device{*port_res};

        auto configure_res = device_.configure(
            dpdk::DeviceConfig{
                .rx_queues = PCAP_EGRESS_RX_QUEUES,
                .tx_queues = PCAP_EGRESS_TX_QUEUES});
        if (!configure_res) {
            return std::unexpected(configure_res.error());
        }

        auto tx_setup_res = device_.setup_tx_queue(queue_id_);
        if (!tx_setup_res) {
            return std::unexpected(tx_setup_res.error());
        }

        auto start_res = device_.start();
        if (!start_res) {
            return std::unexpected(start_res.error());
        }

        queue_ = dpdk::CoreQueue{*port_res, queue_id_};

        return {};
    }

    void shutdown() {
        device_.stop();
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        const std::size_t sent = queue_.tx_burst(packets);

        for (std::size_t i = sent; i < packets.size(); ++i) {
            packets[i].free();
        }

        sent_ += sent;
        dropped_ += packets.size() - sent;

        return packets.subspan(0, sent);
    }

    NodeStats collect_stats() const {
        return NodeStats{
            .node = PCAP_EGRESS_NODE_NAME,
            .counters = {
                {.name = SENT_COUNTER, .value = sent_},
                {.name = DROPPED_COUNTER, .value = dropped_}}};
    }

private:
    std::uint64_t sent_ = 0;
    std::uint64_t dropped_ = 0;
    std::string device_name_;
    std::uint16_t queue_id_ = 0;
    dpdk::Device device_;
    dpdk::CoreQueue queue_;
};

}  // namespace hydralb::data
