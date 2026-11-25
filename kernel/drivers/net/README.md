# Network subsystem

Initial VirtualBox target: Intel PRO/1000 MT Desktop (82540EM), attached to
NAT.

Planned implementation order:

1. PCI configuration-space enumeration.
2. Physical memory allocation and DMA-safe buffers.
3. E1000 device detection and register initialization.
4. Ethernet frame transmit/receive.
5. ARP and IPv4.
6. DHCP and UDP.
7. TCP and DNS, then HTTPS/TLS support in userland.

The package manager must not fetch packages until transport security and
package digest/signature verification are available.
