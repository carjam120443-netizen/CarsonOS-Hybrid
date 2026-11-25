# Network subsystem

Initial target: VirtualBox Intel PRO/1000 MT Desktop (82540EM), attached to NAT.
AT&T home gateways use standard Ethernet, IPv4, DHCP, DNS, and TCP/IP; CarsonOS
does not need a carrier-specific network driver.

This milestone adds PCI memory-space/bus-master enable, E1000 reset and MAC
register checks, aligned static RX/TX descriptor rings and packet buffers,
bootstrap DMA address translation for the current fixed kernel load mapping,
polling receive descriptor recycling, link-state reporting, and the `net`
shell command. The DMA mapping is temporary and must be replaced by a general
physical allocator before arbitrary DMA allocations.

The `carpkg` shell surface includes read-only `help`, `repo`, `list`,
`search`, and `installed` commands. The catalog is currently empty.
Install/remove/update/upgrade are deliberately disabled.

Not implemented yet: Ethernet parsing/transmit, ARP, IPv4, UDP, DHCP, DNS,
TCP, TLS, persistent package database, or package installation. Next: Ethernet
RX/TX and ARP, then IPv4/UDP/DHCP, then DNS/TCP. Package installation requires
secure transport, persistent storage, and integrity verification.
