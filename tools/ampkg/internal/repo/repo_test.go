package repo

import (
	"os"
	"path/filepath"
	"testing"

	"ghl/ampkg/internal/pkg"
)

func TestBuildAndRead(t *testing.T) {
	dir := t.TempDir()
	payload := t.TempDir()
	if err := os.WriteFile(filepath.Join(payload, "f"), []byte("x"), 0644); err != nil {
		t.Fatal(err)
	}
	m := &pkg.Metadata{Name: "a", Version: "1.0-1", Desc: "a", Deps: []string{"b"}}
	f, err := os.Create(filepath.Join(dir, m.Filename()))
	if err != nil {
		t.Fatal(err)
	}
	if err := pkg.Create(f, m, payload); err != nil {
		t.Fatal(err)
	}
	f.Close()

	entries, err := Build(dir)
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != 1 || entries[0].Meta.Name != "a" || entries[0].File != m.Filename() {
		t.Fatalf("entries = %+v", entries)
	}
	if len(entries[0].Meta.Deps) != 1 || entries[0].Meta.Deps[0] != "b" {
		t.Errorf("deps lost: %+v", entries[0].Meta.Deps)
	}

	entries, err = Read(filepath.Join(dir, IndexName))
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != 1 || entries[0].Meta.Name != "a" || entries[0].Meta.Version != "1.0-1" {
		t.Errorf("read back = %+v", entries)
	}
	if entries[0].Meta.Desc != m.Desc {
		t.Errorf("description = %q, want %q", entries[0].Meta.Desc, m.Desc)
	}
}

func TestReadLegacyIndex(t *testing.T) {
	path := filepath.Join(t.TempDir(), IndexName)
	if err := os.WriteFile(path, []byte("a|1.0-1|a.ampkg|b c\n"), 0644); err != nil {
		t.Fatal(err)
	}
	entries, err := Read(path)
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != 1 || len(entries[0].Meta.Deps) != 2 || entries[0].Meta.Desc != "" {
		t.Fatalf("legacy entry = %+v", entries)
	}
}
