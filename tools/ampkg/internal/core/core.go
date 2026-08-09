// Package core wires repos, the database, and the install root together.
package core

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"ghl/ampkg/internal/db"
	"ghl/ampkg/internal/pkg"
	"ghl/ampkg/internal/repo"
	"ghl/ampkg/internal/version"
)

// Config configures operations.
type Config struct {
	Root    string // install root (default "/")
	RepoDir string // directory holding .ampkg files + index
}

// Core performs package operations against a Config.
type Core struct {
	cfg Config
}

// New returns a Core for the given config.
func New(cfg Config) *Core {
	return &Core{cfg: cfg}
}

// Repo loads the repository index.
func (c *Core) Repo() ([]repo.Entry, error) {
	return repo.Read(filepath.Join(c.cfg.RepoDir, repo.IndexName))
}

// Install installs names and their dependencies from the repo.
func (c *Core) Install(names ...string) ([]string, error) {
	entries, err := c.Repo()
	if err != nil {
		return nil, err
	}
	byName := make(map[string]repo.Entry, len(entries))
	for _, e := range entries {
		byName[e.Meta.Name] = e
	}

	// Resolve names + deps into an install plan (deps first).
	var plan []repo.Entry
	seen := make(map[string]bool)
	var add func(name string) error
	add = func(name string) error {
		if seen[name] {
			return nil
		}
		e, ok := byName[name]
		if !ok {
			return fmt.Errorf("unknown package: %s", name)
		}
		seen[name] = true
		for _, dep := range e.Meta.Deps {
			if err := add(dep); err != nil {
				return err
			}
		}
		plan = append(plan, e)
		return nil
	}
	for _, n := range names {
		if err := add(n); err != nil {
			return nil, err
		}
	}

	// Refuse to clobber already-installed packages.
	installed, err := db.New(c.cfg.Root).InstalledNames()
	if err != nil {
		return nil, err
	}
	for _, e := range plan {
		if installed[e.Meta.Name] {
			return nil, fmt.Errorf("%s is already installed", e.Meta.Name)
		}
	}

	// Check file conflicts against what's already installed.
	owned, err := db.New(c.cfg.Root).Ownership()
	if err != nil {
		return nil, err
	}
	claim := make(map[string]string)
	for _, e := range plan {
		m, files, err := pkg.ListFile(filepath.Join(c.cfg.RepoDir, e.File))
		if err != nil {
			return nil, fmt.Errorf("%s: %w", e.File, err)
		}
		_ = m
		for _, f := range files {
			if owner, ok := owned[f]; ok {
				return nil, fmt.Errorf("conflict: %s is owned by %s", f, owner)
			}
			if other, ok := claim[f]; ok && other != e.Meta.Name {
				return nil, fmt.Errorf("conflict: %s is claimed by both %s and %s", f, other, e.Meta.Name)
			}
			claim[f] = e.Meta.Name
		}
	}

	// Install.
	db := db.New(c.cfg.Root)
	var installedNow []string
	for _, e := range plan {
		m, files, err := pkg.ExtractFile(filepath.Join(c.cfg.RepoDir, e.File), c.cfg.Root)
		if err != nil {
			return installedNow, fmt.Errorf("%s: %w", e.Meta.Name, err)
		}
		if err := db.Add(m, files); err != nil {
			return installedNow, err
		}
		installedNow = append(installedNow, e.Meta.Name)
	}
	return installedNow, nil
}

// Remove uninstalls names, refusing when something that is not also being
// removed still depends on them.
func (c *Core) Remove(names ...string) error {
	db := db.New(c.cfg.Root)
	installed, err := db.Installed()
	if err != nil {
		return err
	}

	removing := make(map[string]bool, len(names))
	for _, n := range names {
		removing[n] = true
	}

	for _, n := range names {
		for _, m := range installed {
			if m.Name == n {
				continue
			}
			if removing[m.Name] {
				continue // it goes away too, so no conflict
			}
			for _, dep := range m.Deps {
				if dep == n {
					return fmt.Errorf("cannot remove %s: %s depends on it", n, m.Name)
				}
			}
		}
	}

	owned, err := db.Ownership()
	if err != nil {
		return err
	}

	for _, n := range names {
		files, err := db.Files(n)
		if err != nil {
			return err
		}
		for _, f := range files {
			if owner, ok := owned[f]; ok && !removing[owner] {
				continue // another package owns it
			}
			os.Remove(filepath.Join(c.cfg.Root, f))
		}
		if err := db.Remove(n); err != nil {
			return err
		}
	}
	return nil
}

// Upgrade reinstalls installed packages for which the repo has a newer
// version. Packages that depend on an upgraded package are upgraded too,
// even if the repo doesn't have a newer build of them, so that the system
// never contains a mix of broken old/new pairs. It returns the names
// upgraded, in install order.
func (c *Core) Upgrade() ([]string, error) {
	entries, err := c.Repo()
	if err != nil {
		return nil, err
	}
	inRepo := make(map[string]repo.Entry, len(entries))
	for _, e := range entries {
		inRepo[e.Meta.Name] = e
	}

	installed, err := db.New(c.cfg.Root).Installed()
	if err != nil {
		return nil, err
	}
	installedNames := make(map[string]*pkg.Metadata, len(installed))
	for _, m := range installed {
		installedNames[m.Name] = m
	}

	upgrade := make(map[string]bool)
	for _, m := range installed {
		if e, ok := inRepo[m.Name]; ok && version.Compare(e.Meta.Version, m.Version) > 0 {
			upgrade[m.Name] = true
		}
	}
	if len(upgrade) == 0 {
		return nil, nil
	}

	// Pull in dependents so nothing references a removed version.
	changed := true
	for changed {
		changed = false
		for _, m := range installed {
			if upgrade[m.Name] {
				continue
			}
			if _, ok := inRepo[m.Name]; !ok {
				continue // can't reinstall it; leave it alone
			}
			for _, dep := range m.Deps {
				if upgrade[dep] {
					upgrade[m.Name] = true
					changed = true
					break
				}
			}
		}
	}

	names := make([]string, 0, len(upgrade))
	for n := range upgrade {
		names = append(names, n)
	}
	sort.Strings(names)

	if err := c.Remove(names...); err != nil {
		return nil, err
	}
	return c.Install(names...)
}

// Search matches term against package names and descriptions in the repo.
func (c *Core) Search(term string) ([]repo.Entry, error) {
	entries, err := c.Repo()
	if err != nil {
		return nil, err
	}
	term = strings.ToLower(term)
	var out []repo.Entry
	for _, e := range entries {
		if strings.Contains(strings.ToLower(e.Meta.Name), term) ||
			strings.Contains(strings.ToLower(e.Meta.Desc), term) {
			out = append(out, e)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Meta.Name < out[j].Meta.Name })
	return out, nil
}

// Installed lists installed packages.
func (c *Core) Installed() ([]*pkg.Metadata, error) {
	return db.New(c.cfg.Root).Installed()
}
