// Package db tracks which packages are installed and which files they own.
// State lives under <root>/var/lib/ampkg/local/<name>/.
package db

import (
	"os"
	"path/filepath"
	"sort"
	"strings"

	"ghl/ampkg/internal/pkg"
)

// DirSuffix is where the package database lives inside the root.
const DirSuffix = "var/lib/ampkg/local"

// DB tracks installed packages for a given root.
type DB struct {
	dir string
}

// New returns a DB for the given install root.
func New(root string) *DB {
	return &DB{dir: filepath.Join(root, DirSuffix)}
}

// Add records a freshly installed package.
func (d *DB) Add(m *pkg.Metadata, files []string) error {
	dir := filepath.Join(d.dir, m.Name)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	var info strings.Builder
	m.WritePKGINFO(&info)
	if err := os.WriteFile(filepath.Join(dir, "info"), []byte(info.String()), 0644); err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(dir, "files"), []byte(strings.Join(files, "\n")+"\n"), 0644)
}

// Remove deletes a package's database entry.
func (d *DB) Remove(name string) error {
	return os.RemoveAll(filepath.Join(d.dir, name))
}

// Installed returns metadata for every installed package, sorted by name.
func (d *DB) Installed() ([]*pkg.Metadata, error) {
	entries, err := os.ReadDir(d.dir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var out []*pkg.Metadata
	for _, e := range entries {
		if !e.IsDir() {
			continue
		}
		m, err := readInfo(filepath.Join(d.dir, e.Name(), "info"))
		if err != nil {
			return nil, err
		}
		out = append(out, m)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Name < out[j].Name })
	return out, nil
}

// InstalledNames returns the names of installed packages.
func (d *DB) InstalledNames() (map[string]bool, error) {
	installed, err := d.Installed()
	if err != nil {
		return nil, err
	}
	names := make(map[string]bool, len(installed))
	for _, m := range installed {
		names[m.Name] = true
	}
	return names, nil
}

// Files returns the paths owned by a package.
func (d *DB) Files(name string) ([]string, error) {
	data, err := os.ReadFile(filepath.Join(d.dir, name, "files"))
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}
	var out []string
	for _, line := range strings.Split(string(data), "\n") {
		if line != "" {
			out = append(out, line)
		}
	}
	return out, nil
}

// Ownership maps every installed path to the package that owns it.
func (d *DB) Ownership() (map[string]string, error) {
	installed, err := d.Installed()
	if err != nil {
		return nil, err
	}
	owned := make(map[string]string)
	for _, m := range installed {
		files, err := d.Files(m.Name)
		if err != nil {
			return nil, err
		}
		for _, f := range files {
			owned[f] = m.Name
		}
	}
	return owned, nil
}

func readInfo(path string) (*pkg.Metadata, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return pkg.ParsePKGINFO(f)
}
