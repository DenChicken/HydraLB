#pragma once

#include <gtest/gtest.h>
#include <rte_mempool.h>

import hydralb.dpdk;
import std;

namespace hydralb::test {

class EalEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;

    static ::rte_mempool* pool();

private:
    static inline dpdk::Mempool pool_{};
};

}  // namespace hydralb::test
