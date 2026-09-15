#include <gtest/gtest.h>

#include "suite/eal_environment.hpp"

import hydralb.dpdk;
import std;

namespace hydralb::test {

namespace {

const std::string TEST_POOL_NAME = "HYDRALB_TEST_POOL";

constexpr std::uint32_t TEST_POOL_ELEMENTS = 1023;
constexpr std::uint32_t TEST_POOL_CACHE_SIZE = 32;
constexpr std::int32_t TEST_POOL_SOCKET_ID = -1;

const std::vector<std::string> TEST_EAL_ARGS =
    {"hydralb_integration_tests", "-c", "0x1", "-n", "4", "--no-huge", "--no-pci"};

}  // namespace

void EalEnvironment::SetUp() {
    auto init_ok = dpdk::Eal::init(TEST_EAL_ARGS);
    ASSERT_TRUE(init_ok) << (init_ok ? "" : init_ok.error());

    auto pool_res = dpdk::Mempool::create(
        TEST_POOL_NAME,
        TEST_POOL_ELEMENTS,
        TEST_POOL_CACHE_SIZE,
        TEST_POOL_SOCKET_ID);
    ASSERT_TRUE(pool_res) << (pool_res ? "" : pool_res.error());

    pool_ = *pool_res;
}

void EalEnvironment::TearDown() {
    dpdk::Eal::cleanup();
}

::rte_mempool* EalEnvironment::pool() {
    return pool_.raw();
}

}  // namespace hydralb::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new hydralb::test::EalEnvironment());

    return RUN_ALL_TESTS();
}
