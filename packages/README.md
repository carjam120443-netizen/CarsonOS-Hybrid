# CarsonOS packages

This directory contains package manifests and the local package catalog.

A package manifest will describe a package name, version, architecture,
description, payload path, and cryptographic digest. Payload installation
will be enabled only after the FAT32/VFS and package verification layers are
implemented.

Do not place unverified executable binaries in this directory.
