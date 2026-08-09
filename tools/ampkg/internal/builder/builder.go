// Package builder turns recipes into .ampkg archives.
package builder

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"

	"ghl/ampkg/internal/pkg"
	"ghl/ampkg/internal/recipe"
)

// Build compiles the recipe at recipePath into an .ampkg written to w.
// Source paths in the recipe are resolved relative to buildRoot.
func Build(recipePath, buildRoot string, w io.Writer) (*pkg.Metadata, error) {
	r, err := recipe.Load(recipePath)
	if err != nil {
		return nil, err
	}

	payload, err := os.MkdirTemp("", "ampkg-payload-*")
	if err != nil {
		return nil, err
	}
	defer os.RemoveAll(payload)

	if err := stage(r, buildRoot, payload); err != nil {
		return nil, fmt.Errorf("%s: %w", r.Name, err)
	}

	size, err := pkg.PayloadSize(payload)
	if err != nil {
		return nil, err
	}
	m := &pkg.Metadata{
		Name:      r.Name,
		Version:   fmt.Sprintf("%s-%d", r.Version, r.Rel),
		Desc:      r.Desc,
		Deps:      r.Deps,
		BuildDate: time.Now().Unix(),
		Size:      size,
	}
	if err := pkg.Create(w, m, payload); err != nil {
		return nil, err
	}
	return m, nil
}

func stage(r *recipe.Recipe, buildRoot, payload string) error {
	for _, f := range r.Files {
		src := filepath.Join(buildRoot, filepath.FromSlash(f[0]))
		dst := filepath.Join(payload, filepath.FromSlash(f[1]))
		info, err := os.Stat(src)
		if err != nil {
			return fmt.Errorf("source %s: %w", f[0], err)
		}
		if err := os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
			return err
		}
		if info.IsDir() {
			if err := copyTree(src, dst); err != nil {
				return err
			}
		} else {
			if err := copyFile(src, dst, info.Mode()); err != nil {
				return err
			}
		}
	}
	for _, l := range r.Links {
		dst := filepath.Join(payload, filepath.FromSlash(l[1]))
		if err := os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
			return err
		}
		os.Remove(dst)
		if err := os.Symlink(l[0], dst); err != nil {
			return err
		}
	}
	return nil
}

func copyFile(src, dst string, mode os.FileMode) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, mode.Perm())
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		return err
	}
	return out.Close()
}

func copyTree(src, dst string) error {
	return filepath.Walk(src, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(src, path)
		if err != nil {
			return err
		}
		target := filepath.Join(dst, rel)
		if info.IsDir() {
			return os.MkdirAll(target, info.Mode().Perm())
		}
		return copyFile(path, target, info.Mode())
	})
}
