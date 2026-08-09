package pkg

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
)

func TestRoundTrip(t *testing.T) {
	payload := t.TempDir()
	mk := func(p string, mode os.FileMode) {
		full := filepath.Join(payload, p)
		if err := os.MkdirAll(filepath.Dir(full), 0755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(full, []byte(p+" data"), mode); err != nil {
			t.Fatal(err)
		}
	}
	mk("usr/bin/hello", 0755)
	mk("etc/hello.conf", 0644)
	if err := os.Symlink("hello", filepath.Join(payload, "usr/bin/h")); err != nil {
		t.Fatal(err)
	}

	m := &Metadata{Name: "demo", Version: "1.0-1", Desc: "demo", Deps: []string{"base"}, Size: 1}
	var buf bytes.Buffer
	if err := Create(&buf, m, payload); err != nil {
		t.Fatal(err)
	}

	root := t.TempDir()
	got, files, err := Extract(&buf, root)
	if err != nil {
		t.Fatal(err)
	}
	if got.Name != "demo" || got.Version != "1.0-1" {
		t.Errorf("meta = %+v", got)
	}
	want := []string{"usr/bin/hello", "etc/hello.conf", "usr/bin/h"}
	gotSet := make(map[string]bool, len(files))
	for _, f := range files {
		gotSet[f] = true
	}
	if len(files) != len(want) {
		t.Fatalf("files = %v", files)
	}
	for _, f := range want {
		if !gotSet[f] {
			t.Errorf("missing %q in %v", f, files)
		}
	}

	b, err := os.ReadFile(filepath.Join(root, "usr/bin/hello"))
	if err != nil {
		t.Fatal(err)
	}
	if string(b) != "usr/bin/hello data" {
		t.Errorf("content = %q", b)
	}
	target, err := os.Readlink(filepath.Join(root, "usr/bin/h"))
	if err != nil {
		t.Fatal(err)
	}
	if target != "hello" {
		t.Errorf("link target = %q", target)
	}
}

func TestList(t *testing.T) {
	payload := t.TempDir()
	if err := os.MkdirAll(filepath.Join(payload, "usr/bin"), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(payload, "usr/bin/x"), []byte("x"), 0644); err != nil {
		t.Fatal(err)
	}
	m := &Metadata{Name: "demo", Version: "1.0-1"}
	var buf bytes.Buffer
	if err := Create(&buf, m, payload); err != nil {
		t.Fatal(err)
	}
	_, files, err := List(&buf)
	if err != nil {
		t.Fatal(err)
	}
	if len(files) != 1 || files[0] != "usr/bin/x" {
		t.Errorf("files = %v (directory entries must be excluded)", files)
	}
}
