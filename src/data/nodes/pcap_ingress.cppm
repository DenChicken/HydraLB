module;

#include <rte_ethdev.h>

export module hydralb.data:pcap_ingress;

import :pipeline_node;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::uint16_t PCAP_INGRESS_RX_QUEUES = 1;
constexpr std::uint16_t PCAP_INGRESS_TX_QUEUES = 0;

constexpr std::string_view PCAP_INGRESS_NODE_NAME = "pcap_ingress";

}  // namespace hydralb::data

export namespace hydralb::data {

class PcapIngressNode {
public:
    struct Config {
        std::string device_name;
        std::string mempool_name;
        std::uint16_t queue_id = 0;
    };

    explicit PcapIngressNode(const Config& config)
        : device_name_(config.device_name),
          mempool_name_(config.mempool_name),
          queue_id_(config.queue_id) {}

    std::expected<void, std::string> configure() {
        auto port_res = dpdk::Device::find_by_name(device_name_);
        if (!port_res) {
            return std::unexpected(port_res.error());
        }

        device_ = dpdk::Device{*port_res};

        auto configure_res = device_.configure(
            dpdk::DeviceConfig{
                .rx_queues = PCAP_INGRESS_RX_QUEUES,
                .tx_queues = PCAP_INGRESS_TX_QUEUES});
        if (!configure_res) {
            return std::unexpected(configure_res.error());
        }

        auto mempool_res = dpdk::Mempool::find_by_name(mempool_name_);
        if (!mempool_res) {
            return std::unexpected(std::format("Mempool not found: {}", mempool_name_));
        }

        auto rx_setup_res = device_.setup_rx_queue(queue_id_, *mempool_res);
        if (!rx_setup_res) {
            return std::unexpected(rx_setup_res.error());
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
        const std::size_t received = queue_.rx_burst(packets);
        received_ += received;
        return packets.subspan(0, received);
    }

    NodeStats collect_stats() const {
        return NodeStats{
            .node = PCAP_INGRESS_NODE_NAME,
            .counters = {{.name = RECEIVED_COUNTER, .value = received_}}};
    }

private:
    std::uint64_t received_ = 0;
    std::string device_name_;
    std::string mempool_name_;
    std::uint16_t queue_id_ = 0;
    dpdk::Device device_;
    dpdk::CoreQueue queue_;
};

}  // namespace hydralb::data
