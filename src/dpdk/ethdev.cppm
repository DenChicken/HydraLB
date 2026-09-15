module;

#include <rte_ethdev.h>
#include <rte_mbuf.h>

export module hydralb.dpdk:ethdev;

import :mempool;
import :packet;
import hydralb.common.config;
import std;

export namespace hydralb::dpdk {

constexpr std::uint16_t INVALID_PORT_ID = 0xFFFF;

constexpr std::uint16_t QUEUES_DEFAULT = 1;

constexpr std::uint16_t RX_DESCRIPTORS_DEFAULT = 1024;
constexpr std::uint16_t TX_DESCRIPTORS_DEFAULT = 1024;

struct DeviceConfig {
    std::uint16_t rx_queues = QUEUES_DEFAULT;
    std::uint16_t tx_queues = QUEUES_DEFAULT;
    std::uint16_t rx_descriptors = RX_DESCRIPTORS_DEFAULT;
    std::uint16_t tx_descriptors = TX_DESCRIPTORS_DEFAULT;
};

class Device {
public:
    Device() = default;
    Device(std::uint16_t id) : id_(id) {}

public:
    static std::expected<std::uint16_t, std::string> find_by_name(const std::string& name) {
        std::uint16_t port_id = 0;
        int ret = ::rte_eth_dev_get_port_by_name(name.c_str(), &port_id);
        if (ret < 0) {
            return std::unexpected(std::format("Failed to resolve port by name {}: {}", name, ret));
        }
        return port_id;
    }

    std::expected<void, std::string> configure(const DeviceConfig& config) {
        struct ::rte_eth_conf port_conf{};

        int ret = ::rte_eth_dev_configure(id_, config.rx_queues, config.tx_queues, &port_conf);
        if (ret < 0) {
            return std::unexpected(std::format("Failed to configure eth device {}: {}", id_, ret));
        }

        rx_descriptors_ = config.rx_descriptors;
        tx_descriptors_ = config.tx_descriptors;

        return {};
    }

    std::expected<void, std::string> setup_rx_queue(std::uint16_t queue_id, Mempool& pool) {
        int ret = ::rte_eth_rx_queue_setup(
            id_,
            queue_id,
            rx_descriptors_,
            ::rte_eth_dev_socket_id(id_),
            nullptr,
            pool.raw());

        if (ret < 0) {
            return std::unexpected(
                std::format("Failed to setup RX queue {} on port {}: {}", queue_id, id_, ret));
        }

        return {};
    }

    std::expected<void, std::string> setup_tx_queue(std::uint16_t queue_id) {
        int ret = ::rte_eth_tx_queue_setup(
            id_,
            queue_id,
            tx_descriptors_,
            ::rte_eth_dev_socket_id(id_),
            nullptr);

        if (ret < 0) {
            return std::unexpected(
                std::format("Failed to setup TX queue {} on port {}: {}", queue_id, id_, ret));
        }

        return {};
    }

    std::expected<void, std::string> start() {
        int ret = ::rte_eth_dev_start(id_);
        if (ret < 0) {
            return std::unexpected(std::format("Failed to start eth device {}: {}", id_, ret));
        }

        ::rte_eth_promiscuous_enable(id_);

        return {};
    }

    void stop() {
        if (id_ != INVALID_PORT_ID) {
            ::rte_eth_dev_stop(id_);
        }
    }

private:
    std::uint16_t id_ = INVALID_PORT_ID;
    std::uint16_t rx_descriptors_ = RX_DESCRIPTORS_DEFAULT;
    std::uint16_t tx_descriptors_ = TX_DESCRIPTORS_DEFAULT;
};

class CoreQueue {
public:
    CoreQueue() = default;
    CoreQueue(std::uint16_t port_id, std::uint16_t queue_id)
        : port_id_(port_id), queue_id_(queue_id) {}

public:
    std::size_t rx_burst(std::span<Packet> out_batch) {
        const std::uint16_t to_receive =
            static_cast<std::uint16_t>(std::min(out_batch.size(), config::BURST_SIZE));

        ::rte_mbuf** raw_mbufs_ptr = reinterpret_cast<::rte_mbuf**>(out_batch.data());

        return ::rte_eth_rx_burst(port_id_, queue_id_, raw_mbufs_ptr, to_receive);
    }

    std::size_t tx_burst(std::span<Packet> input_batch) {
        const std::uint16_t to_send =
            static_cast<std::uint16_t>(std::min(input_batch.size(), config::BURST_SIZE));

        ::rte_mbuf** raw_mbufs_ptr = reinterpret_cast<::rte_mbuf**>(input_batch.data());

        return ::rte_eth_tx_burst(port_id_, queue_id_, raw_mbufs_ptr, to_send);
    }

private:
    std::uint16_t port_id_ = INVALID_PORT_ID;
    std::uint16_t queue_id_ = 0;
};

}  // namespace hydralb::dpdk
