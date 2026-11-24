# CarsonOS Hybrid — Development Status

_Last updated: 2026-11-24_

This file is the project's handoff note: it records what is known about the current implementation, what has been tested, what still needs verification, and the next development steps. Update it whenever a milestone changes.

## Project identity

**CarsonOS Hybrid** is an experimental, original x86_64 kernel project. It is not a Linux distribution and does not use the Linux kernel. The current implementation is a small freestanding kernel written in C and NASM, loaded by Limine. The long-term goal is a custom operating system with its own shell, graphical environment, window manager, display/login manager, and applications.

Primary repository: https://github.com/carjam120443-netizen/CarsonOS-Hybrid

## Current status at a glance

| Component | Status | Evidence / caveat |
| --- | --- | --- |
| Limine bootloader and ISO workflow | Implemented | GitHub Actions builds a BIOS + UEFI hybrid ISO using Limine's binary release. |
| x86_64 kernel entry | Confirmed working | Earlier VirtualBox serial log reached `kernel_main()`. |
| COM1 serial logging | Confirmed working | Earlier log successfully printed kernel boot messages. |
| Framebuffer request and drawing code | Implemented, not yet confirmed working | The last reported serial log said `Framebuffer: unavailable`. A later request-ID fix was pushed, but a successful framebuffer response has not yet been confirmed in the available development notes. |
| CarsonOS boot splash | Implemented in source | Depends on successful framebuffer initialization; a black screen was previously reported. |
| CarsonShell prompt and command handling | Present in source, needs VM verification | Current source polls the PS/2 controller and implements `help`, `clear`, and `about`. Do not assume keyboard input works until tested. |
| CarsonWM / desktop | Planned | Not implemented yet. |

**Last known ISO size:** approximately 4.2 MB (reported by the developer for recent builds). This is a historical observation, not a guaranteed size for future builds.

## Confirmed development history

- Limine successfully displayed its boot menu in VirtualBox.
- The kernel was reached and executed. The earlier serial log contained:
  ```text
  CarsonOS Hybrid
  CarsonOS Hybrid kernel
  Architecture: x86_64
  Boot status: kernel entry reached
  Serial console: COM1 ready
  CPU interrupts remain disabled; entering halt loop.
  ```
- The black VirtualBox display at that stage was expected because graphical framebuffer output had not yet been implemented.
- Framebuffer support and a CarsonOS splash screen were then added.
- A later serial log reported:
  ```text
  CarsonOS Hybrid
  Architecture: x86_64
  Boot status: kernel entry reached
  Framebuffer: unavailable
  Serial console: COM1 ready
  CPU interrupts remain disabled; entering halt loop.
  ```
- A follow-up change added the four Limine framebuffer request ID values. The available notes do not establish that this resolved the issue.
- CarsonShell code was subsequently added, including a terminal-style prompt, basic command parsing, and PS/2 keyboard polling.
- Recent Actions run #11 was reported successful, with commit `457ef43` and an ISO artifact named `CarsonOS-Hybrid-x86_64-ISO`. Run #10 was an older failed run. Check Actions for the latest run before downloading an ISO.

## Important commits remembered

These hashes are retained from the development conversation. Verify them in GitHub before relying on them for a checkout or bisect.

| Commit | Remembered change |
| --- | --- |
| `3b79356bc6cec69af744a40d05490e1bc8d84380` | Earlier Limine workflow adjustment |
| `e2c48f17b4fe382a113418ef7aac7f345a6069cf` | Limine binary-release workflow fix |
| `0251f0d` | Limine framebuffer request ID correction |
| `c68b052` | CarsonShell and keyboard input |
| `17ff910` | Freestanding command parsing |
| `457ef43` | Command dispatch fix; associated with the successful Actions run #11 |

The exact commit for the ASCII logo asset and the later framebuffer implementation was not retained in the notes.

## Current implementation details observed in the repository

The following files were inspected on 2026-11-24:

- `kernel/main.c`: Contains the Limine framebuffer request and response structures, framebuffer validation, pixel/rectangle drawing, a small 5×7 font, a terminal-like panel, a basic command parser, and PS/2 keyboard polling.
- `kernel/arch/x86_64/entry.asm`: 64-bit entry point; disables interrupts, aligns the stack, and calls `kernel_main`.
- `kernel/arch/x86_64/cpu.asm`: NASM helpers for port I/O, disabling interrupts, and halting.
- `kernel/include/arch.h`: Declares the architecture helper functions.
- `kernel/linker.ld`: Higher-half kernel layout with a physical load address of 1 MiB; retains Limine request sections.
- `boot/limine.conf`: Limine entry loads `boot():/boot/kernel.elf`.
- `Makefile`: Builds the kernel ELF from C and NASM sources with a freestanding x86_64 GCC toolchain.
- `.github/workflows/build-iso.yml`: Builds the kernel, downloads Limine v12.11.1 binary release assets, assembles a BIOS/UEFI ISO, installs the Limine BIOS stage, and uploads `CarsonOS-Hybrid-x86_64-ISO` for 14 days.
- `assets/carsonos-logo.txt`: CarsonOS ASCII logo asset (previously added to the repository).

### Current shell limitations visible in source

- The keyboard code polls ports `0x64` and `0x60` directly.
- The key map currently covers a small set of letters and space; it is not a complete keyboard layout.
- There is no full keyboard driver, interrupt-driven input, terminal scrollback, process model, or user-space shell yet.
- The current built-in commands are `help`, `clear`, and `about`.
- The current prompt and drawing layout use fixed screen coordinates in places, so different framebuffer resolutions need testing.
- The shell runs inside the kernel; it is not yet a separate user-space program.

## Build and test workflow

1. Open the repository's [GitHub Actions page](https://github.com/carjam120443-netizen/CarsonOS-Hybrid/actions).
2. Select the newest successful **Build CarsonOS Hybrid ISO** run, not an older run.
3. Download the artifact named `CarsonOS-Hybrid-x86_64-ISO`.
4. Test it in VirtualBox before considering any physical-hardware experiment.

Previously used VirtualBox test configuration:

- Type: Other / 64-bit
- RAM: 2 GB
- CPUs: 2
- EFI: enabled for UEFI testing
- PIIX3 chipset
- 64 MB video memory; VBoxSVGA; 3D acceleration disabled
- Attach the ISO as the optical disk; no virtual hard disk is required for this early test
- Audio and networking can remain disabled

For serial diagnostics, use COM1 with I/O port `0x3F8`, IRQ `4`, **Raw File** mode, and a path such as `D:\\temp\\CarsonOS-serial.log`. Create the parent directory first. Do not use Host Pipe mode when the intention is to write a normal log file.

Read the log in PowerShell:

```powershell
Get-Content D:\\temp\\CarsonOS-serial.log -Raw
```

Useful serial milestones to look for:

- `Boot status: kernel entry reached`
- `Framebuffer: initialized` or a clear framebuffer failure message
- `CarsonShell: ready`

A successful GitHub Actions build only proves that the ISO build completed. It does not prove that the ISO boots correctly or that graphics and keyboard input work in VirtualBox.

## Immediate debugging priorities

1. Boot the newest successful ISO and capture the complete COM1 serial log.
2. Determine whether the framebuffer response is still unavailable. If it is, inspect the Limine request declaration, request-section placement/retention in the linker script, base revision, and the bootloader configuration against the Limine protocol version actually bundled by the workflow.
3. If the framebuffer initializes but the screen stays black, inspect framebuffer address, dimensions, pitch, bpp, memory model, RGB mask sizes/shifts, and the pixel-writing path. Add serial diagnostics for those values before changing multiple things at once.
4. Only after the splash is visible, verify keyboard input and the three shell commands. If input fails, diagnose the VirtualBox keyboard/controller path and PS/2 polling before expanding the shell.
5. Keep the serial logger available while graphics and input are developed.

## Roadmap

### Foundation
- [x] Limine boot protocol and kernel entry
- [x] Basic COM1 serial output
- [x] Automated BIOS + UEFI ISO build
- [x] Initial framebuffer drawing implementation
- [x] CarsonOS ASCII logo asset
- [x] Initial CarsonShell code and basic command parser
- [ ] Confirm framebuffer output on a fresh ISO in VirtualBox
- [ ] Confirm keyboard input and shell commands in VirtualBox
- [ ] Improve text rendering, line handling, scrolling, and screen-size handling
- [ ] Add reliable keyboard support and a clean input abstraction

### Kernel and system foundations
- [ ] GDT and IDT setup
- [ ] Interrupt handling and timer
- [ ] Physical and virtual memory management
- [ ] More robust device drivers
- [ ] Filesystem support
- [ ] Process/task management and a system-call interface
- [ ] Separate kernel services from user-space programs

### Graphical environment
- [ ] Reusable framebuffer graphics library
- [ ] Mouse input and cursor
- [ ] CarsonWM: custom window manager with movable and resizable windows
- [ ] CarsonDM: display/login manager (after the necessary session and user infrastructure exists)
- [ ] CarsonDesktop: desktop, panel/taskbar, launcher, clock, wallpaper, and settings
- [ ] Native CarsonOS applications

## Development rules

- Keep the project an original kernel; do not silently turn it into a Linux distribution.
- Prefer complete, coherent file changes over scattered partial edits.
- Keep builds reproducible through the existing GitHub Actions workflow.
- Make one root-cause-focused change at a time when debugging boot, graphics, or input.
- Preserve serial diagnostics until equivalent reliable debugging facilities exist.
- Test experimental ISOs in VirtualBox first. Do not install early builds on the primary PC or on disks containing important data.
- Update this document with the actual result after every meaningful test; distinguish **implemented**, **built**, and **tested successfully**.
