#include <gtest/gtest.h>

import hydralb.algorithms.maglev;
import hydralb.common.config;
import std;

namespace {

using namespace hydralb;

constexpr std::uint32_t BACKEND_IP_BASE = 0x0A00000A;
constexpr std::uint16_t BACKEND_PORT = 80;
constexpr std::uint32_t BACKEND_WEIGHT = 1;

constexpr std::size_t DEFAULT_BACKEND_COUNT = 4;

constexpr double MAX_SHARE_DEVIATION = 0.01;
constexpr double MAX_EXCESS_DISRUPTION = 0.01;

using LookupTable = std::array<std::uint32_t, config::MAGLEV_TABLE_SIZE>;

std::vector<config::Backend> make_backends(std::size_t count) {
    std::vector<config::Backend> backends(count);

    for (std::size_t i = 0; i < count; ++i) {
        backends[i] = config::Backend{
            .id = static_cast<std::uint32_t>(i + 1),
            .ip = {.address = BACKEND_IP_BASE + static_cast<std::uint32_t>(i)},
            .port = BACKEND_PORT,
            .weight = BACKEND_WEIGHT,
            .status = config::BackendStatus::Alive};
    }

    return backends;
}

std::array<std::size_t, config::MAX_BACKENDS> count_slots(const LookupTable& table) {
    std::array<std::size_t, config::MAX_BACKENDS> counts{};

    for (const std::uint32_t slot : table) {
        if (slot != algorithms::LOOKUP_EMPTY_SLOT) {
            ++counts[slot];
        }
    }

    return counts;
}

std::size_t count_empty(const LookupTable& table) {
    return static_cast<std::size_t>(std::ranges::count(table, algorithms::LOOKUP_EMPTY_SLOT));
}

class MaglevTest : public ::testing::Test {
protected:
    static void populate(const std::vector<config::Backend>& backends, LookupTable& target) {
        algorithms::MaglevHasher::populate_table(backends, backends.size(), target);
    }

    LookupTable table{};
};

TEST_F(MaglevTest, FillsEverySlot) {
    populate(make_backends(DEFAULT_BACKEND_COUNT), table);

    EXPECT_EQ(count_empty(table), 0u);
}

TEST_F(MaglevTest, AssignsOnlyKnownBackends) {
    populate(make_backends(DEFAULT_BACKEND_COUNT), table);

    for (const std::uint32_t slot : table) {
        EXPECT_LT(slot, DEFAULT_BACKEND_COUNT);
    }
}

TEST_F(MaglevTest, IsDeterministic) {
    LookupTable other{};

    populate(make_backends(DEFAULT_BACKEND_COUNT), table);
    populate(make_backends(DEFAULT_BACKEND_COUNT), other);

    EXPECT_EQ(table, other);
}

TEST_F(MaglevTest, DistributesEvenly) {
    populate(make_backends(DEFAULT_BACKEND_COUNT), table);

    const auto counts = count_slots(table);
    const double expected_share = 1.0 / static_cast<double>(DEFAULT_BACKEND_COUNT);

    for (std::size_t i = 0; i < DEFAULT_BACKEND_COUNT; ++i) {
        const double share =
            static_cast<double>(counts[i]) / static_cast<double>(config::MAGLEV_TABLE_SIZE);
        EXPECT_NEAR(share, expected_share, MAX_SHARE_DEVIATION);
    }
}

TEST_F(MaglevTest, MovesMinimalSlotsWhenBackendDies) {
    auto backends = make_backends(DEFAULT_BACKEND_COUNT);
    populate(backends, table);

    const auto counts_before = count_slots(table);
    const std::size_t dead_index = 1;
    const std::size_t slots_of_dead = counts_before[dead_index];

    backends[dead_index].status = config::BackendStatus::Dead;

    LookupTable after{};
    populate(backends, after);

    std::size_t moved = 0;
    for (std::size_t i = 0; i < config::MAGLEV_TABLE_SIZE; ++i) {
        if (table[i] != after[i]) {
            ++moved;
        }
    }

    EXPECT_GE(moved, slots_of_dead);

    const double excess =
        static_cast<double>(moved - slots_of_dead) / static_cast<double>(config::MAGLEV_TABLE_SIZE);
    EXPECT_LT(excess, MAX_EXCESS_DISRUPTION);
}

TEST_F(MaglevTest, KeepsDeadBackendOutOfTable) {
    auto backends = make_backends(DEFAULT_BACKEND_COUNT);
    const std::size_t dead_index = 2;
    backends[dead_index].status = config::BackendStatus::Dead;

    populate(backends, table);

    EXPECT_EQ(count_slots(table)[dead_index], 0u);
    EXPECT_EQ(count_empty(table), 0u);
}

TEST_F(MaglevTest, EmptiesTableWhenNoBackendsAlive) {
    auto backends = make_backends(DEFAULT_BACKEND_COUNT);
    for (auto& backend : backends) {
        backend.status = config::BackendStatus::Dead;
    }

    populate(backends, table);

    EXPECT_EQ(count_empty(table), config::MAGLEV_TABLE_SIZE);
}

TEST_F(MaglevTest, EmptiesTableWhenNoBackends) {
    populate({}, table);

    EXPECT_EQ(count_empty(table), config::MAGLEV_TABLE_SIZE);
}

TEST_F(MaglevTest, SendsEverythingToSingleAliveBackend) {
    populate(make_backends(1), table);

    EXPECT_EQ(count_slots(table)[0], config::MAGLEV_TABLE_SIZE);
}

}  // namespace
