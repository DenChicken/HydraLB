module;

#include <rte_bus_vdev.h>
#include <rte_ethdev.h>

export module hydralb.data:pcap_egress;

import :pipeline_node;
import hydralb.common.config;
import hydralb.dpdk;
import std;

namespace hydralb::data {

constexpr std::string_view PCAP_EGRESS_DEVICE_NAME = "net_pcap_egress";
constexpr std::string_view PCAP_EGRESS_TX_ARG_KEY = "tx_pcap";

constexpr dpdk::QueueId PCAP_EGRESS_QUEUE_ID = static_cast<dpdk::QueueId>(0);
constexpr std::uint16_t PCAP_EGRESS_RX_QUEUES = 0;
constexpr std::uint16_t PCAP_EGRESS_TX_QUEUES = 1;

static std::expected<dpdk::PortId, std::string> resolve_port_by_name(const std::string& name) {
    std::uint16_t port_id = 0;
    int ret = ::rte_eth_dev_get_port_by_name(name.c_str(), &port_id);
    if (ret < 0) {
        return std::unexpected(std::format("Failed to resolve port by name {}: {}", name, ret));
    }
    return static_cast<dpdk::PortId>(port_id);
}

}  // namespace hydralb::data

export namespace hydralb::data {

class PcapEgressNode {
public:
    PcapEgressNode(const config::PcapEgressConfig& config) : filename_(config.filename) {}

    std::expected<void, std::string> configure() {
        std::string vdev_args = std::format("{}={}", PCAP_EGRESS_TX_ARG_KEY, filename_);

        int hotplug_ret = ::rte_eal_hotplug_add(
            "vdev",
            std::string(PCAP_EGRESS_DEVICE_NAME).c_str(),
            vdev_args.c_str());

        if (hotplug_ret < 0) {
            return std::unexpected(
                std::format("Failed to hotplug pcap egress device: {}", hotplug_ret));
        }

        auto port_res = resolve_port_by_name(std::string(PCAP_EGRESS_DEVICE_NAME));
        if (!port_res) {
            return std::unexpected(port_res.error());
        }

        device_ = dpdk::Device{*port_res};

        dpdk::DeviceConfig config{};
        config.rx_queues = PCAP_EGRESS_RX_QUEUES;
        config.tx_queues = PCAP_EGRESS_TX_QUEUES;
        config.enable_rss = false;
        config.enable_hw_rx_cksum = false;
        config.enable_hw_tx_cksum = false;

        auto configure_res = device_.configure(config);
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

        dpdk::QueueConfig q_config{};
        q_config.port_id = *port_res;
        q_config.queue_id = PCAP_EGRESS_QUEUE_ID;
        q_config.enable_hw_tx_cksums = false;

        queue_ = dpdk::CoreQueue{q_config};

        return {};
    }

    std::span<dpdk::Packet> process(std::span<dpdk::Packet> packets) {
        std::size_t sent = queue_.tx_burst(packets);

        for (std::size_t i = sent; i < packets.size(); ++i) {
            if (packets[i].is_valid()) {
                packets[i].free();
            }
        }

        return {};
    }

private:
    std::string filename_;
    dpdk::Device device_;
    dpdk::CoreQueue queue_;
};

}  // namespace hydralb::data
