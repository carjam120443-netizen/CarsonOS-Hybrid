# carpkg

CarsonOS's native package manager.

## Planned commands

- `carpkg help`
- `carpkg search <term>`
- `carpkg list`
- `carpkg info <package>`
- `carpkg install <package>`
- `carpkg remove <package>`
- `carpkg update`
- `carpkg upgrade`
- `carpkg installed`

## Package format

Package metadata lives under `packages/`. Repository indexes live under
`repos/`. The stable repository is described by
`repos/stable/index.json`.

The initial implementation must distinguish catalog operations from system
operations. Search and metadata inspection can work before disk and network
drivers exist. Install, remove, update, and upgrade must not report success
until persistent filesystem writes and repository transport are implemented.

## Security requirements

- Validate package names and paths; never trust repository-provided paths.
- Verify package hashes before extraction.
- Reject path traversal and writes outside the package destination.
- Do not execute package scripts as kernel code.
- Keep package metadata separate from package payloads.
