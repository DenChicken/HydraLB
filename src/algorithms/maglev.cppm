export module hydralb.algorithms.maglev;

import hydralb.common.config;
import std;

export namespace hydralb::algorithms {

constexpr std::uint64_t FNV_SEED = 14695981039346656037ULL;
constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;

constexpr std::uint64_t MAGIC_SEED_1 = 0x12345678;
constexpr std::uint64_t MAGIC_SEED_2 = 0x87654321;

constexpr std::uint64_t LOOKUP_INVALID_ID = 0xFFFFFFFF;

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
        std::uint32_t offset = 0;
        std::uint32_t skip = 0;
        std::uint32_t next_index = 0;
    };

public:
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
            std::fill(lookup_table.begin(), lookup_table.end(), LOOKUP_INVALID_ID);
            return;
        }

        std::array<BackendPermutation, config::MAX_BACKENDS> permutations{};

        for (std::size_t i = 0; i < actual_count; ++i) {
            if (backends[i].status == config::BackendStatus::Dead) {
                continue;
            }

            std::uint64_t h1 = fnv1a_hash(&backends[i].id, sizeof(backends[i].id), MAGIC_SEED_1);
            std::uint64_t h2 =
                fnv1a_hash(&backends[i].ip.address, sizeof(backends[i].ip.address), MAGIC_SEED_2);

            permutations[i].offset = static_cast<std::uint32_t>(h1 % config::MAGLEV_TABLE_SIZE);
            permutations[i].skip =
                static_cast<std::uint32_t>(h2 % (config::MAGLEV_TABLE_SIZE - 1) + 1);
            permutations[i].next_index = 0;
        }

        std::fill(lookup_table.begin(), lookup_table.end(), LOOKUP_INVALID_ID);

        std::size_t filled_slots = 0;

        while (filled_slots < config::MAGLEV_TABLE_SIZE) {
            for (std::size_t i = 0; i < actual_count; ++i) {
                if (backends[i].status == config::BackendStatus::Dead) {
                    continue;
                }

                while (true) {
                    std::uint32_t candidate_slot =
                        (permutations[i].offset +
                         permutations[i].next_index * permutations[i].skip) %
                        config::MAGLEV_TABLE_SIZE;

                    permutations[i].next_index++;

                    if (lookup_table[candidate_slot] == LOOKUP_INVALID_ID) {
                        lookup_table[candidate_slot] = backends[i].id;
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
