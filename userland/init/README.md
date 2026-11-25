# CarsonInit

CarsonInit is the first CarsonOS startup coordinator.

## Current implementation

The kernel does not yet have ring-3 process creation, executable loading, or a
system-call ABI. CarsonInit is therefore compiled into the kernel as a small
bootstrap coordinator. It records which boot components start and which are
deferred, without pretending to be a separate user-mode process.

The kernel reports memory, storage, filesystem, PCI/network, framebuffer, input,
desktop, and shell startup through CarsonInit.

## Next transition

Once process management, user address spaces, an executable loader, and system
calls exist, CarsonInit can become the first actual user-mode program and take
over service startup outside the kernel.
