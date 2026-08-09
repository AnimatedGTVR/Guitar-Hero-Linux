package recipe

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoad(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "ampkgfile")
	content := `pkgname = demo
pkgver = 2.1
pkgrel = 3
pkgdesc = "a test package"
depends = (
    busybox
    base
)
files = (
    build/out/demo  usr/bin/demo
)
links = (
    demo  usr/bin/d
)
`
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
	r, err := Load(path)
	if err != nil {
		t.Fatal(err)
	}
	if r.Name != "demo" || r.Version != "2.1" || r.Rel != 3 {
		t.Errorf("got %s %s %d", r.Name, r.Version, r.Rel)
	}
	if r.Desc != "a test package" {
		t.Errorf("desc = %q", r.Desc)
	}
	if len(r.Deps) != 2 || r.Deps[0] != "busybox" || r.Deps[1] != "base" {
		t.Errorf("deps = %v", r.Deps)
	}
	if len(r.Files) != 1 || r.Files[0] != [2]string{"build/out/demo", "usr/bin/demo"} {
		t.Errorf("files = %v", r.Files)
	}
	if len(r.Links) != 1 || r.Links[0] != [2]string{"demo", "usr/bin/d"} {
		t.Errorf("links = %v", r.Links)
	}
}

func TestLoadMissing(t *testing.T) {
	if _, err := Load(filepath.Join(t.TempDir(), "nope")); err == nil {
		t.Error("expected error for missing recipe")
	}
}
