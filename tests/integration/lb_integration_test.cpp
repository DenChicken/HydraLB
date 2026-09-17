#include <gtest/gtest.h>
#include <netinet/in.h>
#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>

#include "suite/integration_suite.hpp"
#include "suite/packet_builder.hpp"

import hydralb.common.config;
import hydralb.common.network;
import hydralb.data;
import std;

namespace hydralb::test {
namespace {

constexpr std::uint32_t LOCAL_TUNNEL_IP = 0x0A000001;
constexpr std::uint32_t FLOW_HASH_SEED = 0x5BD1E995;

constexpr std::uint32_t BACKEND_IP_BASE = 0x0A00000A;
constexpr std::size_t BACKEND_COUNT = 4;

constexpr std::uint32_t CLIENT_IP = 0xC0A80101;
constexpr std::uint32_t SERVICE_IP = 0x0A000064;
constexpr std::uint16_t CLIENT_PORT_BASE = 30000;
constexpr std::uint16_t SERVICE_PORT = 443;

constexpr std::size_t FLOW_COUNT = 16;
constexpr std::size_t PACKETS_PER_FLOW = 4;

config::Balancing make_balancing() {
    config::Balancing balancing;

    balancing.local_tunnel_ip = {.address = LOCAL_TUNNEL_IP};
    balancing.flow_hash_seed = FLOW_HASH_SEED;

    for (std::size_t i = 0; i < BACKEND_COUNT; ++i) {
        balancing.backends.push_back(
            config::Backend{
                .id = static_cast<std::uint32_t>(i + 1),
                .ip = {.address = BACKEND_IP_BASE + static_cast<std::uint32_t>(i)},
                .status = config::BackendStatus::Alive});
    }

    return balancing;
}

FlowSpec make_flow(std::size_t index) {
    return FlowSpec{
        .src_ip = CLIENT_IP,
        .dst_ip = SERVICE_IP,
        .src_port = static_cast<std::uint16_t>(CLIENT_PORT_BASE + index),
        .dst_port = SERVICE_PORT,
        .protocol = network::TransportProtocol::TCP};
}

class IntegrationLBSuite : public IntegrationSuite {
protected:
    void SetUp() override {
        IntegrationSuite::SetUp();

        auto routing_ok = data::setup_routing(make_balancing(), routing_table_);
        ASSERT_TRUE(routing_ok) << (routing_ok ? "" : routing_ok.error());
    }

    data::LBNode make_node() const {
        return data::LBNode{data::LBNode::Config{
            .routing_table = &routing_table_,
            .local_tunnel_ip = LOCAL_TUNNEL_IP,
            .flow_hash_seed = FLOW_HASH_SEED}};
    }

    static std::uint32_t outer_destination(const dpdk::Packet& packet) {
        const auto* outer = packet.data_at<::rte_ipv4_hdr>(sizeof(::rte_ether_hdr));
        return rte_be_to_cpu_32(outer->dst_addr);
    }

    static bool is_known_backend(std::uint32_t address) {
        return address >= BACKEND_IP_BASE && address < BACKEND_IP_BASE + BACKEND_COUNT;
    }

    config::RoutingTable routing_table_;
};

TEST_F(IntegrationLBSuite, EncapsulatesIntoIpipTunnel) {
    std::vector<dpdk::Packet> input{make_ipv4_packet(pool(), make_flow(0))};
    const std::vector<std::uint8_t> original = copy_bytes(input.front());

    std::span<const dpdk::Packet> output = run(make_node(), input);

    ASSERT_EQ(output.size(), 1u);

    const auto& packet = output.front();
    const auto* outer = packet.data_at<::rte_ipv4_hdr>(sizeof(::rte_ether_hdr));

    EXPECT_EQ(rte_be_to_cpu_32(outer->src_addr), LOCAL_TUNNEL_IP);
    EXPECT_TRUE(is_known_backend(rte_be_to_cpu_32(outer->dst_addr)));
    EXPECT_EQ(outer->next_proto_id, IPPROTO_IPIP);

    const std::vector<std::uint8_t> encapsulated = copy_bytes(packet);
    ASSERT_EQ(encapsulated.size(), original.size() + sizeof(::rte_ipv4_hdr));

    const std::size_t inner_offset = sizeof(::rte_ether_hdr) + sizeof(::rte_ipv4_hdr);
    EXPECT_TRUE(
        std::equal(
            original.begin() + sizeof(::rte_ether_hdr),
            original.end(),
            encapsulated.begin() + static_cast<std::ptrdiff_t>(inner_offset)))
        << "inner packet must survive encapsulation byte for byte";
}

TEST_F(IntegrationLBSuite, KeepsFlowOnSingleBackend) {
    std::vector<dpdk::Packet> input;
    for (std::size_t flow = 0; flow < FLOW_COUNT; ++flow) {
        for (std::size_t i = 0; i < PACKETS_PER_FLOW; ++i) {
            input.push_back(make_ipv4_packet(pool(), make_flow(flow)));
        }
    }

    std::span<const dpdk::Packet> output = run(make_node(), input);

    ASSERT_EQ(output.size(), input.size());

    for (std::size_t flow = 0; flow < FLOW_COUNT; ++flow) {
        const std::uint32_t expected = outer_destination(output[flow * PACKETS_PER_FLOW]);

        EXPECT_TRUE(is_known_backend(expected));

        for (std::size_t i = 1; i < PACKETS_PER_FLOW; ++i) {
            EXPECT_EQ(outer_destination(output[flow * PACKETS_PER_FLOW + i]), expected)
                << "packets of one flow must share a backend";
        }
    }
}

TEST_F(IntegrationLBSuite, SpreadsFlowsAcrossBackends) {
    std::vector<dpdk::Packet> input;
    for (std::size_t flow = 0; flow < FLOW_COUNT; ++flow) {
        input.push_back(make_ipv4_packet(pool(), make_flow(flow)));
    }

    std::span<const dpdk::Packet> output = run(make_node(), input);

    ASSERT_EQ(output.size(), input.size());

    std::set<std::uint32_t> used;
    for (const auto& packet : output) {
        used.insert(outer_destination(packet));
    }

    EXPECT_GT(used.size(), 1u) << "flows must not collapse onto a single backend";
}

TEST_F(IntegrationLBSuite, DropsNonIpv4Traffic) {
    std::vector<dpdk::Packet> input{make_arp_packet(pool())};

    EXPECT_TRUE(run(make_node(), input).empty());
}

TEST_F(IntegrationLBSuite, ForwardsUdpFlows) {
    FlowSpec flow = make_flow(0);
    flow.protocol = network::TransportProtocol::UDP;

    std::vector<dpdk::Packet> input{make_ipv4_packet(pool(), flow)};

    std::span<const dpdk::Packet> output = run(make_node(), input);

    ASSERT_EQ(output.size(), 1u);
    EXPECT_TRUE(is_known_backend(outer_destination(output.front())));
}

}  // namespace
}  // namespace hydralb::test
