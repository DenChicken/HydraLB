export module hydralb.common.network;

import std;

export namespace hydralb::network {

struct IPv4Address {
    std::uint32_t address = 0;
};

enum class TransportProtocol : std::uint8_t {
    Unknown,
    TCP = 6,
    UDP = 17,
};

struct FlowKey {
    IPv4Address src_ip;
    IPv4Address dst_ip;
    std::uint16_t src_port = 0;
    std::uint16_t dst_port = 0;
    TransportProtocol protocol = TransportProtocol::Unknown;
};

}  // namespace hydralb::network
