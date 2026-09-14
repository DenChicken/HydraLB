module;

#include <rte_bus_vdev.h>
#include <rte_ethdev.h>

export module hydralb.data:pcap_egress;

import :pipeline_node;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::string_view PCAP_EGRESS_TX_ARG_KEY = "tx_pcap";

constexpr dpdk::QueueId PCAP_EGRESS_QUEUE_ID = static_cast<dpdk::QueueId>(0);
constexpr std::uint16_t PCAP_EGRESS_RX_QUEUES = 0;
constexpr std::uint16_t PCAP_EGRESS_TX_QUEUES = 1;

}  // namespace hydralb::data

export namespace hydralb::data {

class PcapEgressNode {
public:
    PcapEgressNode(const config::PcapEgressConfig& config)
        : device_name_(config.device_name), filename_(config.filename) {}

    std::expected<void, std::string> configure() {
        const std::string vdev_args = std::format("{}={}", PCAP_EGRESS_TX_ARG_KEY, filename_);

        int hotplug_ret = ::rte_eal_hotplug_add(
            std::string(config::VDEV_BUS_NAME).c_str(),
            device_name_.c_str(),
            vdev_args.c_str());

        if (hotplug_ret < 0) {
            return std::unexpected(
                std::format("Failed to hotplug device {}: {}", device_name_, hotplug_ret));
        }

        auto port_res = dpdk::Device::find_by_name(device_name_);
        if (!port_res) {
            return std::unexpected(port_res.error());
        }

        device_ = dpdk::Device{*port_res};

        dpdk::DeviceConfig device_config{};
        device_config.rx_queues = PCAP_EGRESS_RX_QUEUES;
        device_config.tx_queues = PCAP_EGRESS_TX_QUEUES;
        device_config.enable_rss = false;
        device_config.enable_hw_rx_cksum = false;
        device_config.enable_hw_tx_cksum = false;

        auto configure_res = device_.configure(device_config);
        if (!configure_res) {
            return std::unexpected(configure_res.error());
        }

        auto tx_setup_res = device_.setup_tx_queue(PCAP_EGRESS_QUEUE_ID);
        if (!tx_setup_res) {
            return std::unexpected(tx_setup_res.error());
        }

        auto start_res = device_.start();
        if (!start_res) {
            return std::unexpected(start_res.error());
        }

        queue_ = dpdk::CoreQueue{
            dpdk::QueueConfig{.port_id = *port_res, .queue_id = PCAP_EGRESS_QUEUE_ID}};

        return {};
    }

    void shutdown() {
        device_.stop();
        ::rte_eal_hotplug_remove(std::string(config::VDEV_BUS_NAME).c_str(), device_name_.c_str());
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        const std::size_t sent = queue_.tx_burst(packets);

        for (std::size_t i = sent; i < packets.size(); ++i) {
            packets[i].free();
        }

        return packets.subspan(0, sent);
    }

    void dump_stats() const {}

private:
    std::string device_name_;
    std::string filename_;
    dpdk::Device device_;
    dpdk::CoreQueue queue_;
};

}  // namespace hydralb::data
