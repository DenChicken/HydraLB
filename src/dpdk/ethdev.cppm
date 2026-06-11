module;

#include <rte_ethdev.h>
#include <rte_mbuf.h>

export module hydralb.dpdk:ethdev;

import :mempool;
import :packet;
import std;

export namespace hydralb::dpdk {

constexpr uint16_t RX_QUEUES_DEFAULT = 1;
constexpr uint16_t TX_QUEUES_DEFAULT = 1;

constexpr uint16_t RX_DESCRIPTORS_DEFAULT = 1024;
constexpr uint16_t TX_DESCRIPTORS_DEFAULT = 1024;

constexpr std::size_t QUEUE_SIZE_DEFAULT = 32;

enum class PortId : uint16_t { Invalid = 0xFFFF };
enum class QueueId : uint16_t { Invalid = 0xFFFF };

struct DeviceConfig {
    uint16_t rx_queues = RX_QUEUES_DEFAULT;
    uint16_t tx_queues = TX_QUEUES_DEFAULT;
    uint16_t rx_descriptors = RX_DESCRIPTORS_DEFAULT;
    uint16_t tx_descriptors = TX_DESCRIPTORS_DEFAULT;
    bool enable_rss = true;
    uint64_t rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP;
};

class Device {
public:
    Device() = default;
    Device(PortId id) : id_(id) {}

public:
    PortId id() const {
        return id_;
    }

    bool is_valid() const {
        return id_ != PortId::Invalid;
    }

    std::expected<void, std::string> configure(const DeviceConfig& config) {
        struct ::rte_eth_dev_info dev_info{};
        int info_ret = ::rte_eth_dev_info_get(static_cast<uint16_t>(id_), &dev_info);
        if (info_ret < 0) {
            return std::unexpected(
                std::format(
                    "Failed to get device info for port {}: {}",
                    static_cast<uint16_t>(id_),
                    info_ret));
        }

        struct ::rte_eth_conf local_port_conf{};
        if (config.enable_rss) {
            if ((dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_RSS_HASH) == 0) {
                return std::unexpected(
                    std::format(
                        "RSS is requested but not supported by port {}",
                        static_cast<uint16_t>(id_)));
            }
            local_port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
            local_port_conf.rx_adv_conf.rss_conf.rss_key = nullptr;
            local_port_conf.rx_adv_conf.rss_conf.rss_hf =
                config.rss_hf & dev_info.flow_type_rss_offloads;
        }

        const uint64_t requested_tx_offloads =
            RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
            RTE_ETH_TX_OFFLOAD_TCP_CKSUM | RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;

        local_port_conf.txmode.offloads = requested_tx_offloads & dev_info.tx_offload_capa;

        if ((local_port_conf.txmode.offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) == 0) {
            return std::unexpected(
                std::format(
                    "Hardware IPv4 TX checksum offload is required but not supported by port {}",
                    static_cast<uint16_t>(id_)));
        }

        int ret = ::rte_eth_dev_configure(
            static_cast<uint16_t>(id_),
            config.rx_queues,
            config.tx_queues,
            &local_port_conf);
        if (ret < 0) {
            return std::unexpected(
                std::format(
                    "Failed to configure eth device {}: {}",
                    static_cast<uint16_t>(id_),
                    ret));
        }

        rx_descriptors_ = config.rx_descriptors;
        tx_descriptors_ = config.tx_descriptors;

        return {};
    }

    std::expected<void, std::string> setup_rx_queue(QueueId queue_id, Mempool& pool) {
        int ret = ::rte_eth_rx_queue_setup(
            static_cast<uint16_t>(id_),
            static_cast<uint16_t>(queue_id),
            rx_descriptors_,
            ::rte_eth_dev_socket_id(static_cast<uint16_t>(id_)),
            nullptr,
            pool.raw());

        if (ret < 0) {
            return std::unexpected(
                std::format(
                    "Failed to setup RX queue {} on port {}: {}",
                    static_cast<uint16_t>(queue_id),
                    static_cast<uint16_t>(id_),
                    ret));
        }

        return {};
    }

    std::expected<void, std::string> setup_tx_queue(QueueId queue_id) {
        int ret = ::rte_eth_tx_queue_setup(
            static_cast<uint16_t>(id_),
            static_cast<uint16_t>(queue_id),
            tx_descriptors_,
            ::rte_eth_dev_socket_id(static_cast<uint16_t>(id_)),
            nullptr);

        if (ret < 0) {
            return std::unexpected(
                std::format(
                    "Failed to setup TX queue {} on port {}: {}",
                    static_cast<uint16_t>(queue_id),
                    static_cast<uint16_t>(id_),
                    ret));
        }

        return {};
    }

    std::expected<void, std::string> start() {
        int ret = ::rte_eth_dev_start(static_cast<uint16_t>(id_));
        if (ret < 0) {
            return std::unexpected(
                std::format("Failed to start eth device {}: {}", static_cast<uint16_t>(id_), ret));
        }

        ::rte_eth_promiscuous_enable(static_cast<uint16_t>(id_));

        return {};
    }

    void stop() {
        if (is_valid()) {
            ::rte_eth_dev_stop(static_cast<uint16_t>(id_));
        }
    }

private:
    PortId id_ = PortId::Invalid;
    uint16_t rx_descriptors_ = RX_DESCRIPTORS_DEFAULT;
    uint16_t tx_descriptors_ = TX_DESCRIPTORS_DEFAULT;
};

class CoreQueue {
public:
    CoreQueue() = default;
    CoreQueue(PortId port, QueueId queue) : port_(port), queue_(queue) {}

public:
    std::size_t rx_burst(std::span<Packet> out_batch) {
        const uint16_t to_receive =
            static_cast<uint16_t>(std::min(out_batch.size(), QUEUE_SIZE_DEFAULT));
        if (to_receive == 0) {
            return 0;
        }

        ::rte_mbuf** raw_mbufs_ptr = reinterpret_cast<::rte_mbuf**>(out_batch.data());

        return ::rte_eth_rx_burst(
            static_cast<uint16_t>(port_),
            static_cast<uint16_t>(queue_),
            raw_mbufs_ptr,
            to_receive);
    }

    std::size_t tx_burst(std::span<Packet> input_batch) {
        ::rte_mbuf* raw_mbufs[QUEUE_SIZE_DEFAULT];

        std::size_t to_send = std::min(input_batch.size(), std::size_t(QUEUE_SIZE_DEFAULT));

        for (std::size_t i = 0; i < to_send; ++i) {
            raw_mbufs[i] = input_batch[i].mbuf();
        }

        uint16_t sent = ::rte_eth_tx_burst(
            static_cast<uint16_t>(port_),
            static_cast<uint16_t>(queue_),
            raw_mbufs,
            static_cast<uint16_t>(to_send));

        return sent;
    }

private:
    PortId port_ = PortId::Invalid;
    QueueId queue_ = QueueId::Invalid;
};

}  // namespace hydralb::dpdk
