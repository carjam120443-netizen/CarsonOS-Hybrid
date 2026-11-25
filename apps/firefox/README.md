# Firefox integration

CarsonOS currently includes a Firefox launcher entry in CarsonWM and a `firefox` shell command.

**This is a launcher scaffold, not the Firefox browser itself.** The current kernel cannot execute Firefox yet.

Before a real Firefox build can run, CarsonOS needs:
- user-mode processes and a stable executable format/loader;
- a syscall ABI, memory isolation, and a C/POSIX-compatible runtime (or a deliberate native port);
- working network drivers, packet handling, TCP/IP, DNS, and sockets;
- a graphics/windowing API, input events, timers, and font/rendering support;
- storage writes, package installation, and TLS/certificate support.

The intended integration is to package a verified Firefox build for a supported runtime, register it as the `firefox` desktop application, and launch it through CarsonOS's process manager. Do not label the current placeholder as a functioning browser.
