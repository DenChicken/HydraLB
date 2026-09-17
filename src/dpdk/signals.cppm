module;

#include <csignal>
#include <rte_debug.h>
#include <unistd.h>

export module hydralb.dpdk:signals;

import std;

namespace hydralb::dpdk {

constexpr int CRASH_EXIT_CODE = 128;

std::atomic<bool> stop_requested{false};

extern "C" void handle_stop(int) {
    stop_requested.store(true, std::memory_order_relaxed);
}

extern "C" void handle_crash(int signal_number) {
    ::rte_dump_stack();
    ::_exit(CRASH_EXIT_CODE + signal_number);
}

std::expected<void, std::string> install(int signal_number, void (*handler)(int), int flags) {
    struct ::sigaction action{};
    action.sa_handler = handler;
    action.sa_flags = flags;
    ::sigemptyset(&action.sa_mask);

    if (::sigaction(signal_number, &action, nullptr) < 0) {
        return std::unexpected(
            std::format("Failed to install handler for signal {}", signal_number));
    }

    return {};
}

}  // namespace hydralb::dpdk

export namespace hydralb::dpdk {

class Signals {
public:
    static std::expected<void, std::string> install_handlers() {
        for (const int signal_number : {SIGINT, SIGTERM}) {
            auto stop_ok = install(signal_number, handle_stop, 0);
            if (!stop_ok) {
                return stop_ok;
            }
        }

        for (const int signal_number : {SIGSEGV, SIGABRT}) {
            auto crash_ok = install(signal_number, handle_crash, SA_RESETHAND);
            if (!crash_ok) {
                return crash_ok;
            }
        }

        return {};
    }

    static const std::atomic<bool>& stop_flag() {
        return stop_requested;
    }
};

}  // namespace hydralb::dpdk
