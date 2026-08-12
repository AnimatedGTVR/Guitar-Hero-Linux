# ampkg

GHL's command-line package manager. It is a static Go binary with no runtime
dependencies. The separate `installer` command owns system installation and
setup; ampkg only builds, installs, removes, searches, and upgrades packages.

**Scope:** package manager only. Partitioning, base-system install, and bootloader stuff live elsewhere.

## Status

Working package manager: build and index repositories, install dependency
trees, remove packages safely, search, and upgrade installed packages.

## Layout

```
cmd/ampkg/       main()
internal/core/   package transactions
```

## Dev

```sh
go run ./cmd/ampkg
go build -o ampkg ./cmd/ampkg
```

## Design notes

- Packages are `.ampkg` files: a compressed tar with file metadata + payload
- Repos are directories with an index, served over file/http
- Installed-package database lives in `/var/lib/ampkg/`
- Details get documented in `docs/` as they're designed

## Ideas

- Transaction log so interrupted installs can roll back
