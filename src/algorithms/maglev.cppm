export module hydralb.algorithms.maglev;

import hydralb.common.config;
import std;

export namespace hydralb::algorithms {

constexpr std::uint64_t FNV_SEED = 14695981039346656037ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;

constexpr std::uint64_t OFFSET_SEED = 0xC2B2AE3D27D4EB4F;
constexpr std::uint64_t SKIP_SEED = 0x9E3779B97F4A7C15;

constexpr std::uint32_t LOOKUP_EMPTY_SLOT = 0xFFFFFFFF;

std::uint64_t fnv1a_hash(const void* data, std::size_t size, std::uint64_t seed = FNV_SEED) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = seed;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

class MaglevHasher {
private:
    struct BackendPermutation {
        std::uint32_t next_slot = 0;
        std::uint32_t skip = 0;
    };

public:
    static std::uint32_t lookup(
        std::uint32_t flow_hash,
        const std::array<std::uint32_t, config::MAGLEV_TABLE_SIZE>& lookup_table) {
        return lookup_table[flow_hash % config::MAGLEV_TABLE_SIZE];
    }

    static void populate_table(
        std::span<const config::Backend> backends,
        std::size_t backend_count,
        std::array<std::uint32_t, config::MAGLEV_TABLE_SIZE>& lookup_table) {
        const std::size_t actual_count =
            std::min({backend_count, backends.size(), config::MAX_BACKENDS});

        const std::size_t alive_count =
            std::count_if(backends.begin(), backends.begin() + actual_count, [](const auto& b) {
                return b.status == config::BackendStatus::Alive;
            });

        if (actual_count == 0 || alive_count == 0) {
            std::fill(lookup_table.begin(), lookup_table.end(), LOOKUP_EMPTY_SLOT);
            return;
        }

        std::array<BackendPermutation, config::MAX_BACKENDS> permutations{};

        for (std::size_t i = 0; i < actual_count; ++i) {
            if (backends[i].status == config::BackendStatus::Dead) {
                continue;
            }

            const std::uint64_t offset_hash =
                fnv1a_hash(&backends[i].id, sizeof(backends[i].id), OFFSET_SEED);
            const std::uint64_t skip_hash =
                fnv1a_hash(&backends[i].ip.address, sizeof(backends[i].ip.address), SKIP_SEED);

            permutations[i].next_slot =
                static_cast<std::uint32_t>(offset_hash % config::MAGLEV_TABLE_SIZE);
            permutations[i].skip =
                static_cast<std::uint32_t>(skip_hash % (config::MAGLEV_TABLE_SIZE - 1) + 1);
        }

        std::fill(lookup_table.begin(), lookup_table.end(), LOOKUP_EMPTY_SLOT);

        std::size_t filled_slots = 0;

        while (filled_slots < config::MAGLEV_TABLE_SIZE) {
            for (std::size_t i = 0; i < actual_count; ++i) {
                if (backends[i].status == config::BackendStatus::Dead) {
                    continue;
                }

                while (true) {
                    const std::uint32_t candidate_slot = permutations[i].next_slot;

                    permutations[i].next_slot = static_cast<std::uint32_t>(
                        (candidate_slot + permutations[i].skip) % config::MAGLEV_TABLE_SIZE);

                    if (lookup_table[candidate_slot] == LOOKUP_EMPTY_SLOT) {
                        lookup_table[candidate_slot] = static_cast<std::uint32_t>(i);
                        filled_slots++;
                        break;
                    }
                }

                if (filled_slots >= config::MAGLEV_TABLE_SIZE) {
                    break;
                }
            }
        }
    }
};

}  // namespace hydralb::algorithms
