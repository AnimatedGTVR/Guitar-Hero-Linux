// Package version compares GHL package versions of the form
// "pkgver-pkgrel", e.g. "1.38.0-1". Segments are compared numerically
// when possible and lexically otherwise.
package version

import (
	"strconv"
	"strings"
)

// Compare returns -1, 0, or 1 comparing a and b.
func Compare(a, b string) int {
	as := split(a)
	bs := split(b)
	for i := 0; i < len(as) || i < len(bs); i++ {
		var av, bv string
		if i < len(as) {
			av = as[i]
		}
		if i < len(bs) {
			bv = bs[i]
		}
		if c := cmpSeg(av, bv); c != 0 {
			return c
		}
	}
	return 0
}

// split breaks a version into comparable segments, treating "." and "-"
// as separators.
func split(v string) []string {
	return strings.FieldsFunc(v, func(r rune) bool { return r == '.' || r == '-' })
}

func cmpSeg(a, b string) int {
	// A missing segment sorts before a present one: "1.0" < "1.0.1".
	if a == "" {
		return -1
	}
	if b == "" {
		return 1
	}
	ai, aerr := strconv.Atoi(a)
	bi, berr := strconv.Atoi(b)
	if aerr == nil && berr == nil {
		if ai < bi {
			return -1
		}
		if ai > bi {
			return 1
		}
		return 0
	}
	if a < b {
		return -1
	}
	if a > b {
		return 1
	}
	return 0
}
