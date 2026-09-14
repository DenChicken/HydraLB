export module hydralb.common.network;

import std;

export namespace hydralb::network {

struct IPv4Address {
    std::uint32_t address = 0;
};

enum class TransportProtocol : std::uint32_t {
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

static_assert(
    sizeof(FlowKey) ==
        sizeof(IPv4Address) * 2 + sizeof(std::uint16_t) * 2 + sizeof(TransportProtocol),
    "FlowKey must have no padding: it is hashed as a whole");

}  // namespace hydralb::network
