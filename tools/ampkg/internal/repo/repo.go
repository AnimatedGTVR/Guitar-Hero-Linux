// Package repo builds and reads the repository index. A repo is a
// directory of .ampkg files plus an index listing what's available.
package repo

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"ghl/ampkg/internal/pkg"
)

// IndexName is the index file inside a repo directory.
const IndexName = "index"

// Entry is one package available in a repo.
type Entry struct {
	Meta *pkg.Metadata
	File string // archive filename
}

// Read parses an index file. Each line is:
//
//	name|version|archive-file|dep1 dep2 ...
func Read(path string) ([]Entry, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var entries []Entry
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.SplitN(line, "|", 4)
		if len(parts) < 3 {
			continue
		}
		e := Entry{
			Meta: &pkg.Metadata{
				Name:    parts[0],
				Version: parts[1],
			},
			File: parts[2],
		}
		if len(parts) == 4 && parts[3] != "" {
			e.Meta.Deps = strings.Fields(parts[3])
		}
		entries = append(entries, e)
	}
	return entries, sc.Err()
}

// Write serializes entries to the index file.
func Write(path string, entries []Entry) error {
	sort.Slice(entries, func(i, j int) bool { return entries[i].Meta.Name < entries[j].Meta.Name })
	var b strings.Builder
	b.WriteString("# GHL repo index\n")
	for _, e := range entries {
		fmt.Fprintf(&b, "%s|%s|%s|%s\n",
			e.Meta.Name, e.Meta.Version, e.File, strings.Join(e.Meta.Deps, " "))
	}
	return os.WriteFile(path, []byte(b.String()), 0644)
}

// Build scans dir for .ampkg archives and writes the index.
func Build(dir string) ([]Entry, error) {
	files, err := filepath.Glob(filepath.Join(dir, "*"+pkg.FileExt))
	if err != nil {
		return nil, err
	}
	var entries []Entry
	for _, f := range files {
		m, err := pkg.ReadMeta(f)
		if err != nil {
			return nil, fmt.Errorf("repo: %s: %w", filepath.Base(f), err)
		}
		entries = append(entries, Entry{Meta: m, File: filepath.Base(f)})
	}
	if err := Write(filepath.Join(dir, IndexName), entries); err != nil {
		return nil, err
	}
	return entries, nil
}
