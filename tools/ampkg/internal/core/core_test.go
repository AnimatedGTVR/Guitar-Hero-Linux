package core

import (
	"os"
	"path/filepath"
	"testing"

	"ghl/ampkg/internal/pkg"
	"ghl/ampkg/internal/repo"
)

func writePkg(t *testing.T, repoDir, name, version string, deps []string, payload map[string]string) {
	t.Helper()
	payloadDir := t.TempDir()
	for rel, content := range payload {
		full := filepath.Join(payloadDir, rel)
		if err := os.MkdirAll(filepath.Dir(full), 0755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(full, []byte(content), 0644); err != nil {
			t.Fatal(err)
		}
	}
	m := &pkg.Metadata{Name: name, Version: version, Desc: name, Deps: deps}
	f, err := os.Create(filepath.Join(repoDir, m.Filename()))
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	if err := pkg.Create(f, m, payloadDir); err != nil {
		t.Fatal(err)
	}
}

func newTestCore(t *testing.T) (*Core, string) {
	t.Helper()
	root := t.TempDir()
	repoDir := t.TempDir()
	writePkg(t, repoDir, "busybox", "1.0-1", nil, map[string]string{"usr/bin/busybox": "bb"})
	writePkg(t, repoDir, "base", "1.0-1", []string{"busybox"}, map[string]string{"usr/etc/ghl-release": "ghl"})
	c := New(Config{Root: root, RepoDir: repoDir})
	if _, err := repo.Build(repoDir); err != nil {
		t.Fatal(err)
	}
	return c, root
}

func TestInstallAndRemove(t *testing.T) {
	c, root := newTestCore(t)

	done, err := c.Install("base")
	if err != nil {
		t.Fatal(err)
	}
	// deps installed first, then base
	if len(done) != 2 || done[0] != "busybox" || done[1] != "base" {
		t.Fatalf("done = %v", done)
	}
	for _, f := range []string{"usr/bin/busybox", "usr/etc/ghl-release"} {
		if _, err := os.Stat(filepath.Join(root, f)); err != nil {
			t.Errorf("%s missing: %v", f, err)
		}
	}

	// reinstall refuses
	if _, err := c.Install("base"); err == nil {
		t.Error("expected reinstall to fail")
	}

	// dependency guard
	if err := c.Remove("busybox"); err == nil {
		t.Error("expected removing a dependency to fail")
	}

	if err := c.Remove("base"); err != nil {
		t.Fatal(err)
	}
	if err := c.Remove("busybox"); err != nil {
		t.Fatal(err)
	}
	for _, f := range []string{"usr/bin/busybox", "usr/etc/ghl-release"} {
		if _, err := os.Stat(filepath.Join(root, f)); !os.IsNotExist(err) {
			t.Errorf("%s should be gone, got %v", f, err)
		}
	}
	installed, err := c.Installed()
	if err != nil {
		t.Fatal(err)
	}
	if len(installed) != 0 {
		t.Errorf("installed = %v", installed)
	}
}

func TestInstallUnknown(t *testing.T) {
	c, _ := newTestCore(t)
	if _, err := c.Install("nope"); err == nil {
		t.Error("expected error for unknown package")
	}
}

func TestSearch(t *testing.T) {
	c, _ := newTestCore(t)
	matches, err := c.Search("base")
	if err != nil {
		t.Fatal(err)
	}
	if len(matches) != 1 || matches[0].Meta.Name != "base" {
		t.Errorf("matches = %+v", matches)
	}
	if _, err := c.Search("zzz"); err != nil {
		t.Fatal(err)
	}
}
