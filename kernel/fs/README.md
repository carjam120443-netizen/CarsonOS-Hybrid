# Filesystem subsystem

This milestone adds primary-master ATA PIO IDENTIFY and bounded LBA28 sector
reads, FAT32 volume discovery from an MBR partition (types 0x0B/0x0C) or a
superfloppy boot sector, and read-only file lookup using FAT32 8.3 short names.

The first test target is a new, blank, disposable VDI attached as the primary
IDE master in VirtualBox. Format that VDI as FAT32 and copy
`PACKAGES/INDEX.JSON` onto it. Never attach or format a host physical disk.

This milestone is read-only: it does not create, modify, or delete files. It
does not yet support FAT long filenames, GPT, exFAT, NTFS, or LBA48 disks.
FAT32 writes, a general VFS, and package installation are later milestones.
