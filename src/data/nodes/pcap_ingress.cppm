module;

#include <rte_bus_vdev.h>
#include <rte_ethdev.h>

export module hydralb.data:pcap_ingress;

import :pipeline_node;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::string_view PCAP_INGRESS_RX_ARG_KEY = "rx_pcap";

constexpr std::uint16_t PCAP_INGRESS_QUEUE_ID = 0;
constexpr std::uint16_t PCAP_INGRESS_RX_QUEUES = 1;
constexpr std::uint16_t PCAP_INGRESS_TX_QUEUES = 0;

}  // namespace hydralb::data

export namespace hydralb::data {

class PcapIngressNode {
public:
    PcapIngressNode(const config::PcapIngressConfig& config)
        : device_name_(config.device_name),
          filename_(config.filename),
          mempool_name_(config.mempool_name) {}

    std::expected<void, std::string> configure() {
        const std::string vdev_args = std::format("{}={}", PCAP_INGRESS_RX_ARG_KEY, filename_);

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

        auto rx_setup_res = device_.setup_rx_queue(PCAP_INGRESS_QUEUE_ID, *mempool_res);
        if (!rx_setup_res) {
            return std::unexpected(rx_setup_res.error());
        }

        auto start_res = device_.start();
        if (!start_res) {
            return std::unexpected(start_res.error());
        }

        queue_ = dpdk::CoreQueue{*port_res, PCAP_INGRESS_QUEUE_ID};

        return {};
    }

    void shutdown() {
        device_.stop();
        ::rte_eal_hotplug_remove(std::string(config::VDEV_BUS_NAME).c_str(), device_name_.c_str());
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        const std::size_t received = queue_.rx_burst(packets);
        return packets.subspan(0, received);
    }

    void dump_stats() const {}

private:
    std::string device_name_;
    std::string filename_;
    std::string mempool_name_;
    dpdk::Device device_;
    dpdk::CoreQueue queue_;
};

}  // namespace hydralb::data
