package version

import "testing"

func TestCompare(t *testing.T) {
	cases := []struct {
		a, b string
		want int
	}{
		{"1.0-1", "1.0-1", 0},
		{"1.0-1", "1.0-2", -1},
		{"1.0-2", "1.0-1", 1},
		{"1.0-1", "1.1-1", -1},
		{"1.1-1", "1.0-1", 1},
		{"1.38.0-1", "1.38.0-1", 0},
		{"1.9-1", "1.10-1", -1},
		{"1.0-1", "1.0.1-1", -1},
		{"1.0.1-1", "1.0-1", 1},
		{"2.0-1", "10.0-1", -1},
		{"2.0-1", "2.0-1", 0},
		{"0.1-1", "0.1-2", -1},
	}
	for _, c := range cases {
		if got := Compare(c.a, c.b); got != c.want {
			t.Errorf("Compare(%q, %q) = %d, want %d", c.a, c.b, got, c.want)
		}
	}
}
