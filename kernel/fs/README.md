# Filesystem subsystem

Planned implementation order:

1. Block-device interface.
2. ATA PIO sector read/write for the VirtualBox IDE test disk.
3. FAT32 volume discovery and safe read-only mount.
4. VFS path and file abstractions.
5. FAT32 write support after read-only validation.
6. Package database and atomic package installation.

The first test disk must be a disposable dynamically allocated VDI. Never point
experimental filesystem code at a host physical disk.
