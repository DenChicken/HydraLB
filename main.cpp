import std;
import hydralb.dpdk;
import hydralb.data;

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

    auto pcap_pipeline = hydralb::data::Pipeline{
        hydralb::data::PcapIngressNode{"input.pcap"},
        hydralb::data::L2ReflectorNode{},
        hydralb::data::PcapEgressNode{"output.pcap"},
    };

    std::array<hydralb::dpdk::Packet, 32> lcore_batch{};

    auto configure_ok = pcap_pipeline.configure();
    if (!configure_ok) {
        std::println(std::cerr, "Error: {}", configure_ok.error());
        return 1;
    }

    std::println("Entering static pipeline processing loop...");

    for (int i = 0; i < 5; ++i) {
        pcap_pipeline.process(lcore_batch);
    }

    std::println("Pipeline simulation finished successfully. Cleaning up...");

    hydralb::dpdk::Eal::cleanup();

    return 0;
}
