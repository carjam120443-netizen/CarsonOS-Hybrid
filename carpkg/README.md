# carpkg

CarsonOS has two related interfaces:

1. The in-OS `carpkg` shell commands for reading the mounted package catalog.
2. The host-side `tools/carpkg.py` utility for repository maintenance and ISO builds.

## Host-side subcommands

```sh
python3 tools/carpkg.py help
python3 tools/carpkg.py repo
python3 tools/carpkg.py list
python3 tools/carpkg.py validate
python3 tools/carpkg.py update
python3 tools/carpkg.py build-iso --limine-dir /path/to/limine
```

`update` fetches the Git remote configured in `packages/repository.json`
and replaces only the local `packages/` directory after validation.
`build-iso` embeds that same directory at `/PACKAGES` in the bootable ISO.

## In-OS commands

- `carpkg help`
- `carpkg repo`
- `carpkg list`
- `carpkg search`
- `carpkg installed`
- `carpkg install`
- `carpkg remove`
- `carpkg update`
- `carpkg upgrade`

The in-OS catalog reader currently reads `PACKAGES/INDEX.JSON` from a
mounted FAT32 data volume. The package directory is included in the ISO, but
the kernel still needs an ISO9660 reader or Limine module integration to read
that embedded copy directly. The E1000 driver is not yet a TCP/TLS/HTTPS stack,
so in-OS repository downloads are not enabled. Use the host-side `update`
subcommand until the kernel networking and storage layers are complete.

Package installation/removal remains disabled until persistent filesystem
writes, package hash verification, and safe extraction are implemented.
