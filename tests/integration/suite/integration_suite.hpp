#pragma once

#include <gtest/gtest.h>
#include <rte_mempool.h>

#include "suite/eal_environment.hpp"
#include "suite/test_nodes.hpp"

import hydralb.common.config;
import hydralb.data;
import hydralb.dpdk;
import std;

namespace hydralb::test {

class IntegrationSuite : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = EalEnvironment::pool();
        available_before_ = ::rte_mempool_avail_count(pool_);
    }

    void TearDown() override {
        for (auto& packet : collected_) {
            packet.free();
        }
        collected_.clear();

        EXPECT_EQ(::rte_mempool_avail_count(pool_), available_before_)
            << "mbufs leaked during the test";
    }

    ::rte_mempool* pool() const {
        return pool_;
    }

    template <typename Node>
    std::span<const dpdk::Packet> run(Node&& node, std::span<const dpdk::Packet> input) {
        data::Pipeline<SourceNode, std::decay_t<Node>, SinkNode> pipeline{
            SourceNode{input},
            std::forward<Node>(node),
            SinkNode{collected_},
        };

        auto configure_ok = pipeline.configure();
        EXPECT_TRUE(configure_ok) << (configure_ok ? "" : configure_ok.error());

        std::array<dpdk::Packet, config::BURST_SIZE> batch{};
        for (std::size_t sent = 0; sent < input.size(); sent += config::BURST_SIZE) {
            pipeline.process(batch);
        }

        pipeline.shutdown();

        return collected_;
    }

private:
    std::vector<dpdk::Packet> collected_;
    ::rte_mempool* pool_ = nullptr;
    std::uint32_t available_before_ = 0;
};

}  // namespace hydralb::test
