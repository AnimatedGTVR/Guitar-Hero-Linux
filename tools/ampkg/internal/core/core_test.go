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

func TestInstallSkipsSatisfiedDependency(t *testing.T) {
	c, root := newTestCore(t)
	if _, err := c.Install("busybox"); err != nil {
		t.Fatal(err)
	}
	done, err := c.Install("base")
	if err != nil {
		t.Fatal(err)
	}
	if len(done) != 1 || done[0] != "base" {
		t.Fatalf("installed = %v, want only base", done)
	}
	if _, err := os.Stat(filepath.Join(root, "usr/etc/ghl-release")); err != nil {
		t.Fatal(err)
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

func TestUpgrade(t *testing.T) {
	root := t.TempDir()

	// repo1: old versions. Install base (pulls busybox 1.0).
	repo1 := t.TempDir()
	writePkg(t, repo1, "busybox", "1.0-1", nil, map[string]string{"usr/bin/busybox": "bb"})
	writePkg(t, repo1, "base", "1.0-1", []string{"busybox"}, map[string]string{"usr/etc/ghl-release": "ghl"})
	if _, err := repo.Build(repo1); err != nil {
		t.Fatal(err)
	}
	c1 := New(Config{Root: root, RepoDir: repo1})
	if _, err := c1.Install("base"); err != nil {
		t.Fatal(err)
	}

	// Nothing newer yet -> no-op.
	done, err := c1.Upgrade()
	if err != nil {
		t.Fatal(err)
	}
	if len(done) != 0 {
		t.Fatalf("expected no-op, got %v", done)
	}

	// repo2: newer busybox. base is a dependent, so it's reinstalled too.
	repo2 := t.TempDir()
	writePkg(t, repo2, "busybox", "2.0-1", nil, map[string]string{"usr/bin/busybox": "bb2"})
	writePkg(t, repo2, "base", "1.0-1", []string{"busybox"}, map[string]string{"usr/etc/ghl-release": "ghl"})
	if _, err := repo.Build(repo2); err != nil {
		t.Fatal(err)
	}
	c2 := New(Config{Root: root, RepoDir: repo2})
	done, err = c2.Upgrade()
	if err != nil {
		t.Fatal(err)
	}
	if len(done) != 2 {
		t.Fatalf("upgrade = %v, want 2 packages", done)
	}

	installed, err := c2.Installed()
	if err != nil {
		t.Fatal(err)
	}
	byName := map[string]string{}
	for _, m := range installed {
		byName[m.Name] = m.Version
	}
	if byName["busybox"] != "2.0-1" {
		t.Errorf("busybox version = %q, want 2.0-1", byName["busybox"])
	}
	b, err := os.ReadFile(filepath.Join(root, "usr/bin/busybox"))
	if err != nil {
		t.Fatal(err)
	}
	if string(b) != "bb2" {
		t.Errorf("busybox content = %q, want bb2", b)
	}
}
