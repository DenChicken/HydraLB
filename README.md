# HydraLB

A high-performance L3 load balancer built on DPDK with a Direct Server Return (DSR) forwarding model.

Packets are forwarded in userspace, bypassing the kernel network stack: 
the balancer rewrites only the outer headers and sends traffic to the selected backend, 
while responses go straight from the backend to the client.

## Architecture

The system is split into three layers:
- **Data plane** - per-core packet processing on DPDK, lock-free hot path.
- **Control plane** - backend pool, health state and balancing configuration.
- **Management** - configuration and observability for the operator.

Written in modern C++23 (concepts, modules) as a showcase of both systems programming and contemporary language practice.

## Project Status
This project is currently under active development. 
- [x] Data Plane
- [ ] Control Plane
- [ ] Management
