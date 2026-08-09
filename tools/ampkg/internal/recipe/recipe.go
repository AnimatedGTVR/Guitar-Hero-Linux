// Package recipe parses ampkg recipe files. Recipes are declarative:
//
//	pkgname = busybox
//	pkgver = 1.38.0
//	pkgrel = 1
//	pkgdesc = "GHL base userspace"
//	depends = (
//	    base
//	)
//	files = (
//	    build/out/busybox  usr/bin/busybox
//	)
//	links = (
//	    busybox  usr/bin/sh
//	)
//
// Source paths in files= are relative to the build root passed to the
// builder; destination paths are relative to the package root ("/").
package recipe

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// Recipe describes how to build one package.
type Recipe struct {
	Name    string
	Version string
	Rel     int
	Desc    string
	Deps    []string
	Files   [][2]string // source -> destination
	Links   [][2]string // target -> link name
}

// File is the recipe filename inside a package directory.
const File = "ampkgfile"

// Load parses the recipe file at path.
func Load(path string) (*Recipe, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	r := &Recipe{}
	sc := bufio.NewScanner(f)
	listKey := ""
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if listKey != "" {
			if line == ")" {
				listKey = ""
				continue
			}
			fields := strings.Fields(line)
			if len(fields) < 1 {
				continue
			}
			switch listKey {
			case "depends":
				r.Deps = append(r.Deps, fields[0])
			case "files":
				if len(fields) >= 2 {
					r.Files = append(r.Files, [2]string{fields[0], fields[1]})
				}
			case "links":
				if len(fields) >= 2 {
					r.Links = append(r.Links, [2]string{fields[0], fields[1]})
				}
			}
			continue
		}
		if strings.HasSuffix(line, "(") {
			listKey = strings.TrimSpace(strings.TrimSuffix(strings.TrimSpace(strings.TrimSuffix(line, "(")), "="))
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
			r.Name = val
		case "pkgver":
			r.Version = val
		case "pkgrel":
			r.Rel, _ = strconv.Atoi(val)
		case "pkgdesc":
			r.Desc = strings.Trim(val, `"`)
		}
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	if r.Name == "" || r.Version == "" {
		return nil, fmt.Errorf("recipe %s: missing pkgname or pkgver", path)
	}
	if r.Rel == 0 {
		r.Rel = 1
	}
	return r, nil
}
