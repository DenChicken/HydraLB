module;

#include <rte_eal.h>

export module hydralb.dpdk:eal;

import std;

export namespace hydralb::dpdk {

class Eal {
public:
    static std::expected<int, std::string> init(const std::vector<std::string>& args) {
        std::vector<char*> c_args;
        c_args.reserve(args.size());
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }

        int parsed_args = ::rte_eal_init(static_cast<int>(c_args.size()), c_args.data());
        if (parsed_args < 0) {
            return std::unexpected(std::format("EAL init failed with code: {}", parsed_args));
        }

        return parsed_args;
    }

    static void cleanup() {
        ::rte_eal_cleanup();
    }
};

}  // namespace hydralb::dpdk
