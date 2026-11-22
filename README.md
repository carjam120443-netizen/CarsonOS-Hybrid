# CarsonOS Hybrid

**CarsonOS Hybrid** is an experimental operating-system project exploring a hybrid design: an original kernel with a Linux-inspired userland and familiar developer tooling.

The initial target is **x86_64 PCs**, with development and testing focused on virtual machines before any physical-hardware experiments.

> **Project status:** Early planning and scaffolding. The kernel is not yet bootable, and the features below are goals rather than completed functionality.

## Project goals

- Develop an original x86_64 kernel.
- Boot through the Limine bootloader using the Limine boot protocol.
- Build kernel components primarily in C, with NASM assembly where low-level entry or CPU support requires it.
- Establish memory management, interrupt handling, basic drivers, and a system-call interface.
- Create a small userland with system services, command-line utilities, and the CarsonShell command interpreter.
- Work toward filesystem support, process management, and eventually a graphical desktop.
- Produce reproducible build scripts and a bootable ISO for virtual-machine testing.

## Design overview

The intended system is divided into layers:

1. **Boot:** Limine loads the kernel and supplies boot information.
2. **Kernel:** CPU setup, interrupts, memory management, drivers, and core scheduling/runtime facilities.
3. **System interface:** A defined system-call boundary between kernel and user programs.
4. **Userland:** CarsonOS services, libraries, shell, and command-line utilities.
5. **Desktop:** A future graphical environment and applications.

Linux and other open-source operating systems may inform design decisions, but CarsonOS Hybrid is not a Linux distribution or a Linux-kernel fork. Third-party components will retain their own licenses and notices.

## Planned technology

| Area | Initial choice |
| --- | --- |
| CPU architecture | x86_64 |
| Kernel languages | C and NASM |
| Bootloader / protocol | Limine |
| Build system | GNU Make |
| Kernel toolchain | x86_64-elf cross-compiler |
| Initial testing | QEMU |
| Additional testing | VirtualBox |
| Initial boot target | UEFI |
| Initial userland | Minimal custom userland |

These choices may evolve as the project develops.

## Repository layout

```text
CarsonOS-Hybrid/
├── boot/                 # Bootloader configuration and assets
├── kernel/
│   ├── arch/x86_64/      # Architecture-specific code
│   ├── drivers/          # Device drivers
│   ├── include/          # Kernel headers
│   └── mm/               # Memory management
├── userland/
│   ├── coreutils/        # Basic command-line utilities
│   ├── services/         # User-space services
│   └── shell/            # CarsonShell
├── libraries/            # Shared user-space libraries
├── filesystem/           # Filesystem code and tools
├── desktop/              # Future graphical environment
├── apps/                 # Future applications
├── scripts/              # Build and development scripts
├── build/                # Generated build output
└── iso/                  # ISO staging/output
```

## Development environment

The intended development environment is Linux (a native Linux installation or WSL2), with Windows-hosted virtual machines used for testing if desired.

Typical tools include:

- GCC and binutils, including an `x86_64-elf` cross-toolchain
- NASM
- GNU Make
- Git
- Limine
- xorriso and mtools
- QEMU and GDB

The exact dependency list and build instructions will be documented once the first kernel build is implemented.

## Current roadmap

- [ ] Establish the cross-compilation toolchain and build system
- [ ] Add Limine configuration and kernel entry point
- [ ] Boot a minimal kernel in QEMU
- [ ] Print kernel diagnostics and inspect boot information
- [ ] Add GDT/IDT and interrupt handling
- [ ] Implement physical and virtual memory management
- [ ] Add basic drivers and a kernel debugging interface
- [ ] Define the system-call ABI
- [ ] Start CarsonShell and basic userland utilities
- [ ] Add filesystem and process support
- [ ] Explore a graphical desktop
- [ ] Produce documented bootable ISO releases

## Safety and testing

CarsonOS Hybrid is experimental software. Test early builds in an isolated virtual machine with disposable virtual disks. Do not use experimental images on a primary computer or on disks containing important data.

No physical-disk installation procedure is provided at this stage.

## Contributing

Issues, design discussions, and focused pull requests are welcome. Please describe the change, its rationale, and how it was tested. Keep generated build artifacts out of source commits unless a release explicitly requires them.

## License

The original project files in this repository are licensed under the MIT License; see [LICENSE](LICENSE).

Third-party code, tools, bootloader components, and assets remain subject to their respective licenses. Their required notices must be preserved.
