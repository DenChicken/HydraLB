module;

#include <rte_dev.h>

export module hydralb.data:bootstrap;

import hydralb.algorithms.maglev;
import hydralb.common.config;
import hydralb.dpdk;
import std;

export namespace hydralb::data {

std::expected<void, std::string> validate_startup(const config::StartupConfig& startup) {
    if (startup.workers.empty()) {
        return std::unexpected("No workers configured");
    }

    std::set<std::uint32_t> lcores;
    std::set<std::pair<std::string, std::uint16_t>> bindings;
    std::set<std::string> used_devices;
    std::set<std::string> used_mempools;

    const auto known_device = [&startup](const std::string& name) {
        return std::ranges::contains(startup.devices, name, &config::DeviceConfig::name);
    };

    for (const auto& worker : startup.workers) {
        if (!lcores.insert(worker.lcore).second) {
            return std::unexpected(std::format("Duplicate worker lcore {}", worker.lcore));
        }

        if (!std::ranges::contains(
                startup.mempools,
                worker.mempool,
                &config::MempoolConfig::name)) {
            return std::unexpected(
                std::format(
                    "Worker on lcore {} uses unknown mempool {}",
                    worker.lcore,
                    worker.mempool));
        }
        used_mempools.insert(worker.mempool);

        for (const auto& binding : {worker.rx, worker.tx}) {
            if (!known_device(binding.device)) {
                return std::unexpected(
                    std::format(
                        "Worker on lcore {} uses unknown device {}",
                        worker.lcore,
                        binding.device));
            }

            if (!bindings.insert({binding.device, binding.queue}).second) {
                return std::unexpected(
                    std::format(
                        "Queue {} of device {} is claimed twice",
                        binding.queue,
                        binding.device));
            }

            used_devices.insert(binding.device);
        }
    }

    for (const auto& device : startup.devices) {
        if (!used_devices.contains(device.name)) {
            return std::unexpected(std::format("Device {} is not used by any worker", device.name));
        }
    }

    for (const auto& mempool : startup.mempools) {
        if (!used_mempools.contains(mempool.name)) {
            return std::unexpected(
                std::format("Mempool {} is not used by any worker", mempool.name));
        }
    }

    return {};
}

std::expected<void, std::string> setup_memory(const config::StartupConfig& startup) {
    for (const auto& pool_config : startup.mempools) {
        auto pool_res = dpdk::Mempool::create(
            pool_config.name,
            pool_config.elements,
            pool_config.cache,
            pool_config.socket_id);
        if (!pool_res) {
            return std::unexpected(pool_res.error());
        }
    }

    return {};
}

void teardown_devices(const config::StartupConfig& startup) {
    for (const auto& device : startup.devices) {
        ::rte_eal_hotplug_remove(config::VDEV_BUS_NAME, device.name.c_str());
    }
}

std::expected<void, std::string> setup_devices(const config::StartupConfig& startup) {
    for (const auto& device : startup.devices) {
        int hotplug_ret =
            ::rte_eal_hotplug_add(config::VDEV_BUS_NAME, device.name.c_str(), device.args.c_str());

        if (hotplug_ret < 0) {
            teardown_devices(startup);
            return std::unexpected(
                std::format("Failed to hotplug device {}: {}", device.name, hotplug_ret));
        }
    }

    return {};
}

std::expected<void, std::string> setup_routing(
    const config::RuntimeConfig& runtime,
    config::RoutingTable& routing_table) {
    if (runtime.backends.empty()) {
        return std::unexpected("No backends configured");
    }
    if (runtime.backends.size() > config::MAX_BACKENDS) {
        return std::unexpected(
            std::format(
                "Backend count {} exceeds maximum {}",
                runtime.backends.size(),
                config::MAX_BACKENDS));
    }

    std::ranges::copy(runtime.backends, routing_table.backends.begin());
    routing_table.backend_count = runtime.backends.size();

    algorithms::MaglevHasher::populate_table(
        routing_table.backends,
        routing_table.backend_count,
        routing_table.lookup_table);

    routing_table.is_ready.store(true, std::memory_order_release);

    return {};
}

}  // namespace hydralb::data
