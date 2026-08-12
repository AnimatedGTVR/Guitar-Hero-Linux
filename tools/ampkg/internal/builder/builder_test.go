package builder

import (
	"os"
	"path/filepath"
	"testing"
)

func TestCopyTreePreservesSymlinks(t *testing.T) {
	src, dst := t.TempDir(), filepath.Join(t.TempDir(), "copy")
	if err := os.WriteFile(filepath.Join(src, "target"), []byte("ok"), 0644); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink("target", filepath.Join(src, "link")); err != nil {
		t.Fatal(err)
	}
	if err := copyTree(src, dst); err != nil {
		t.Fatal(err)
	}
	got, err := os.Readlink(filepath.Join(dst, "link"))
	if err != nil {
		t.Fatalf("copied link is not a symlink: %v", err)
	}
	if got != "target" {
		t.Fatalf("link target = %q, want target", got)
	}
}
