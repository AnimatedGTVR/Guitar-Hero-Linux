// Package pkg implements the .ampkg package format: a gzipped tar
// containing a .PKGINFO metadata file plus a root/ tree that gets
// unpacked onto the target system.
package pkg

import (
	"archive/tar"
	"bufio"
	"bytes"
	"compress/gzip"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

const (
	// PKGINFO is the metadata file inside every .ampkg archive.
	PKGINFO = ".PKGINFO"
	// RootDir is the payload directory inside the archive.
	RootDir = "root"
	// FileExt is the package archive extension.
	FileExt = ".ampkg"
)

// Metadata describes a package.
type Metadata struct {
	Name      string
	Version   string // includes pkgrel, e.g. "1.38.0-1"
	Desc      string
	Deps      []string
	BuildDate int64
	Size      int64 // payload size in bytes
}

// Filename returns the canonical archive name for this package.
func (m *Metadata) Filename() string {
	return m.Name + "-" + m.Version + FileExt
}

// WritePKGINFO serializes the metadata in key = value form.
func (m *Metadata) WritePKGINFO(w io.Writer) error {
	fmt.Fprintf(w, "pkgname = %s\n", m.Name)
	fmt.Fprintf(w, "pkgver = %s\n", m.Version)
	fmt.Fprintf(w, "pkgdesc = %s\n", m.Desc)
	sorted := append([]string(nil), m.Deps...)
	sort.Strings(sorted)
	for _, d := range sorted {
		fmt.Fprintf(w, "depends = %s\n", d)
	}
	fmt.Fprintf(w, "builddate = %d\n", m.BuildDate)
	fmt.Fprintf(w, "packsizer = %d\n", m.Size)
	return nil
}

// ParsePKGINFO reads metadata from key = value text.
func ParsePKGINFO(r io.Reader) (*Metadata, error) {
	m := &Metadata{}
	sc := bufio.NewScanner(r)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		key := strings.TrimSpace(parts[0])
		val := strings.TrimSpace(parts[1])
		switch key {
		case "pkgname":
			m.Name = val
		case "pkgver":
			m.Version = val
		case "pkgdesc":
			m.Desc = val
		case "depends":
			m.Deps = append(m.Deps, val)
		case "builddate":
			fmt.Sscanf(val, "%d", &m.BuildDate)
		case "packsizer":
			fmt.Sscanf(val, "%d", &m.Size)
		}
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	if m.Name == "" || m.Version == "" {
		return nil, errors.New("pkg: .PKGINFO is missing pkgname or pkgver")
	}
	return m, nil
}

// Create writes the package described by m, with the payload taken from
// payloadDir (treated as the package's "/"), to w as a gzipped tar.
func Create(w io.Writer, m *Metadata, payloadDir string) error {
	gz := gzip.NewWriter(w)
	defer gz.Close()
	tw := tar.NewWriter(gz)
	defer tw.Close()

	var infoBuf bytes.Buffer
	if err := m.WritePKGINFO(&infoBuf); err != nil {
		return err
	}
	if err := tw.WriteHeader(&tar.Header{
		Name:    PKGINFO,
		Mode:    0644,
		Size:    int64(infoBuf.Len()),
		ModTime: time.Unix(m.BuildDate, 0),
	}); err != nil {
		return err
	}
	if _, err := tw.Write(infoBuf.Bytes()); err != nil {
		return err
	}

	return filepath.Walk(payloadDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if path == payloadDir {
			return nil
		}
		rel, err := filepath.Rel(payloadDir, path)
		if err != nil {
			return err
		}
		name := filepath.Join(RootDir, filepath.ToSlash(rel))

		hdr, err := tar.FileInfoHeader(info, "")
		if err != nil {
			return err
		}
		hdr.Name = name
		if info.IsDir() {
			hdr.Name += "/"
		}
		if info.Mode()&os.ModeSymlink != 0 {
			if hdr.Linkname, err = os.Readlink(path); err != nil {
				return err
			}
		}
		hdr.Uid, hdr.Gid = 0, 0
		hdr.Uname, hdr.Gname = "root", "root"
		hdr.ModTime = time.Unix(m.BuildDate, 0)
		if err := tw.WriteHeader(hdr); err != nil {
			return err
		}
		if !info.Mode().IsRegular() {
			return nil
		}
		f, err := os.Open(path)
		if err != nil {
			return err
		}
		defer f.Close()
		_, err = io.Copy(tw, f)
		return err
	})
}

// extractName converts an archive path under RootDir into a system path
// (relative to the target root) and rejects anything escaping RootDir.
func extractName(name string) (string, error) {
	if name == RootDir {
		return "", nil
	}
	if !strings.HasPrefix(name, RootDir+"/") {
		return "", fmt.Errorf("pkg: unexpected path %q in archive", name)
	}
	rel := strings.TrimPrefix(name, RootDir+"/")
	clean := filepath.Clean(rel)
	if clean == ".." || strings.HasPrefix(clean, "../") {
		return "", fmt.Errorf("pkg: path %q escapes the root", name)
	}
	return filepath.FromSlash(clean), nil
}

// Extract unpacks an .ampkg from r into rootDir. It returns the metadata
// and the list of installed paths (relative to rootDir).
func Extract(r io.Reader, rootDir string) (*Metadata, []string, error) {
	gz, err := gzip.NewReader(r)
	if err != nil {
		return nil, nil, err
	}
	defer gz.Close()
	tr := tar.NewReader(gz)

	var m *Metadata
	var files []string
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, nil, err
		}
		if hdr.Name == PKGINFO {
			var buf bytes.Buffer
			if _, err := io.Copy(&buf, tr); err != nil {
				return nil, nil, err
			}
			m, err = ParsePKGINFO(&buf)
			if err != nil {
				return nil, nil, err
			}
			continue
		}
		rel, err := extractName(hdr.Name)
		if err != nil {
			return nil, nil, err
		}
		if rel == "" {
			continue
		}
		dst := filepath.Join(rootDir, rel)

		switch hdr.Typeflag {
		case tar.TypeDir:
			if err := os.MkdirAll(dst, os.FileMode(hdr.Mode)); err != nil {
				return nil, nil, err
			}
		case tar.TypeReg, tar.TypeRegA:
			if err := os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
				return nil, nil, err
			}
			f, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, os.FileMode(hdr.Mode))
			if err != nil {
				return nil, nil, err
			}
			if _, err := io.Copy(f, tr); err != nil {
				f.Close()
				return nil, nil, err
			}
			if err := f.Close(); err != nil {
				return nil, nil, err
			}
			files = append(files, rel)
		case tar.TypeSymlink:
			if err := os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
				return nil, nil, err
			}
			os.Remove(dst)
			if err := os.Symlink(hdr.Linkname, dst); err != nil {
				return nil, nil, err
			}
			files = append(files, rel)
		}
	}
	if m == nil {
		return nil, nil, errors.New("pkg: archive contains no .PKGINFO")
	}
	return m, files, nil
}

// List reads an .ampkg and returns its metadata plus the paths it would
// install, without writing anything.
func List(r io.Reader) (*Metadata, []string, error) {
	gz, err := gzip.NewReader(r)
	if err != nil {
		return nil, nil, err
	}
	defer gz.Close()
	tr := tar.NewReader(gz)

	var m *Metadata
	var files []string
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, nil, err
		}
		if hdr.Name == PKGINFO {
			var buf bytes.Buffer
			if _, err := io.Copy(&buf, tr); err != nil {
				return nil, nil, err
			}
			m, err = ParsePKGINFO(&buf)
			if err != nil {
				return nil, nil, err
			}
			continue
		}
		rel, err := extractName(hdr.Name)
		if err != nil {
			return nil, nil, err
		}
		switch hdr.Typeflag {
		case tar.TypeReg, tar.TypeRegA, tar.TypeSymlink:
			if rel != "" {
				files = append(files, rel)
			}
		}
	}
	if m == nil {
		return nil, nil, errors.New("pkg: archive contains no .PKGINFO")
	}
	return m, files, nil
}

// ReadMeta reads just the metadata out of an .ampkg file on disk.
func ReadMeta(path string) (*Metadata, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	m, _, err := List(f)
	return m, err
}

// ListFile lists an .ampkg file on disk without extracting anything.
func ListFile(path string) (*Metadata, []string, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, nil, err
	}
	defer f.Close()
	return List(f)
}

// ExtractFile is Extract for a file on disk.
func ExtractFile(path, rootDir string) (*Metadata, []string, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, nil, err
	}
	defer f.Close()
	return Extract(f, rootDir)
}

// PayloadSize sums the regular-file sizes under payloadDir.
func PayloadSize(payloadDir string) (int64, error) {
	var total int64
	err := filepath.Walk(payloadDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.Mode().IsRegular() {
			total += info.Size()
		}
		return nil
	})
	return total, err
}
