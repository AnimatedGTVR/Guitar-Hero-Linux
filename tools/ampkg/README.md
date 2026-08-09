# ampkg

GHL's package manager. It's a **TUI** because there's no desktop on a guitar — and no mouse either.

Written in Go with [Bubble Tea](https://github.com/charmbracelet/bubbletea) + [Lip Gloss](https://github.com/charmbracelet/lipgloss). Static binary, no runtime deps.

**Scope:** package manager only. Partitioning, base-system install, and bootloader stuff live elsewhere.

## Status

Scaffold. The TUI opens to a menu; install/remove/search/etc. are still stubs.

## Layout

```
cmd/ampkg/       main()
internal/tui/    the Bubble Tea UI
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

- Non-interactive mode (`ampkg -S foo`) alongside the TUI
- Transaction log so interrupted installs can roll back
