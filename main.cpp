import std;
import hydralb.dpdk;

int main() {
    std::vector<std::string> eal_args = {"HydraLB", "-c", "0xf", "-n", "4"};

    auto init_ok = hydralb::dpdk::Eal::init(eal_args);
    if (!init_ok) {
        std::println(std::cerr, "Error: {}", init_ok.error());
        return 1;
    }

    auto mempool_res = hydralb::dpdk::Mempool::create("HYDRALB_MBUF_POOL", 8191, 256);
    if (!mempool_res) {
        std::println(std::cerr, "Error: {}", mempool_res.error());
        return 1;
    }

    std::println("DPDK EAL and Mempool partitions loaded successfully");

    hydralb::dpdk::Eal::cleanup();

    return 0;
}
