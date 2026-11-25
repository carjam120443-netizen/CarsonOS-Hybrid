# CarsonOS package repository

This directory is the **single source of truth** for CarsonOS packages and
the stable repository configuration.

- `index.json` is the package catalog.
- `repository.json` stores the Git remote, branch, catalog path, and ISO path.
- Package manifests and payloads belong under this directory.
- The ISO builder copies this entire directory to `/PACKAGES` in every ISO.

Use the host-side tool:

```sh
python3 tools/carpkg.py repo
python3 tools/carpkg.py list
python3 tools/carpkg.py validate
python3 tools/carpkg.py update
python3 tools/carpkg.py build-iso --limine-dir /path/to/limine
```

The updater fetches the configured Git branch and replaces only this
`packages/` directory after validating the downloaded catalog. It does not
change the current branch or other OS source files.

The kernel currently reads `PACKAGES/INDEX.JSON` from a FAT32 data volume.
The ISO also contains the package directory, but the kernel does not yet have
an ISO9660 reader or Limine-module catalog loader. That runtime bridge is a
separate filesystem/boot milestone.

Do not add unverified executable payloads. Before installation is enabled,
packages must have validated paths and SHA-256 digests, and the kernel needs
persistent write support and a verified extraction path.
