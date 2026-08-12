package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"ghl/ampkg/internal/builder"
	"ghl/ampkg/internal/core"
	"ghl/ampkg/internal/repo"
)

const usageText = `ampkg — GHL package manager

usage:
  ampkg                     show this help
  ampkg build <recipe>...   build .ampkg packages from recipes
  ampkg repo-add <dir>      (re)build the repo index in <dir>
  ampkg install <pkg>...    install packages and their dependencies
  ampkg remove <pkg>...     remove packages
  ampkg upgrade            upgrade installed packages
  ampkg search <term>       search the repo
  ampkg list                list installed packages

flags (accepted before, after, or between arguments):
  -r         install root (default $AMPKG_ROOT or /)
  -repo      repo directory (default $AMPKG_REPO or ./repo)
  -o         output directory for built packages (build only)
  -src       source root for relative recipe paths (build only)
`

// splitFlags pulls "-name value" pairs out of args in any position and
// returns the remaining positional arguments.
func splitFlags(args []string, names map[string]bool) (vals map[string]string, positional []string) {
	vals = make(map[string]string)
	for i := 0; i < len(args); i++ {
		a := args[i]
		if len(a) > 1 && a[0] == '-' && a != "--" {
			name := a[1:]
			if len(name) > 1 && name[0] == '-' {
				name = name[1:]
			}
			if names[name] && i+1 < len(args) {
				vals[name] = args[i+1]
				i++
				continue
			}
		}
		positional = append(positional, a)
	}
	return
}

func env(name, def string) string {
	if v := os.Getenv(name); v != "" {
		return v
	}
	return def
}

func main() {
	if len(os.Args) < 2 {
		fmt.Print(usageText)
		return
	}

	vals, args := splitFlags(os.Args[1:], map[string]bool{"r": true, "repo": true, "o": true, "src": true})
	if len(args) == 0 {
		fmt.Print(usageText)
		return
	}
	cmd, rest := args[0], args[1:]

	var err error
	switch cmd {
	case "build":
		err = cmdBuild(vals, rest)
	case "repo-add":
		dir := "."
		if len(rest) > 0 {
			dir = rest[0]
		}
		err = cmdRepoAdd(dir)
	case "install":
		err = cmdInstall(vals, rest)
	case "remove":
		err = cmdRemove(vals, rest)
	case "upgrade":
		err = cmdUpgrade(vals)
	case "search":
		err = cmdSearch(vals, rest)
	case "list":
		err = cmdList(vals)
	default:
		fmt.Fprint(os.Stderr, usageText)
		os.Exit(2)
	}
	run(err)
}

func run(err error) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "ampkg: %v\n", err)
		os.Exit(1)
	}
}

// animate runs a transaction behind a small fret-board animation. Output
// stays plain when redirected, making ampkg safe for scripts and build logs.
func animate(label string, fn func() error) error {
	info, _ := os.Stdout.Stat()
	interactive := info != nil && info.Mode()&os.ModeCharDevice != 0 && os.Getenv("AMPKG_NO_ANIMATION") == ""
	if !interactive {
		return fn()
	}

	done := make(chan error, 1)
	go func() { done <- fn() }()
	frames := []string{"[o----]", "[-o---]", "[--o--]", "[---o-]", "[----o]", "[---o-]", "[--o--]", "[-o---]"}
	ticker := time.NewTicker(70 * time.Millisecond)
	defer ticker.Stop()
	started, frame := time.Now(), 0
	finished := false
	var result error
	fmt.Print("\033[?25l")
	defer fmt.Print("\033[?25h")
	for !finished || time.Since(started) < 420*time.Millisecond {
		fmt.Printf("\r\033[1;36m%s\033[0m  %s", frames[frame%len(frames)], label)
		frame++
		select {
		case result = <-done:
			finished = true
		case <-ticker.C:
		}
	}
	fmt.Print("\r\033[2K")
	if result == nil {
		fmt.Printf("\033[1;32m[done]\033[0m %s\n", label)
	}
	return result
}

func config(vals map[string]string) core.Config {
	return core.Config{
		Root:    orDefault(vals, "r", env("AMPKG_ROOT", "/")),
		RepoDir: orDefault(vals, "repo", env("AMPKG_REPO", "./repo")),
	}
}

func orDefault(vals map[string]string, key, def string) string {
	if v, ok := vals[key]; ok {
		return v
	}
	return def
}

func cmdBuild(vals map[string]string, recipes []string) error {
	if len(recipes) == 0 {
		return fmt.Errorf("build: need at least one recipe directory")
	}
	out := orDefault(vals, "o", env("AMPKG_REPO", "./repo"))
	src := orDefault(vals, "src", ".")
	if err := os.MkdirAll(out, 0755); err != nil {
		return err
	}
	for _, dir := range recipes {
		recipePath := filepath.Join(dir, "ampkgfile")
		dst := filepath.Join(out, filepath.Base(dir)+".ampkg")
		f, err := os.Create(dst)
		if err != nil {
			return err
		}
		m, err := builder.Build(recipePath, src, f)
		f.Close()
		if err != nil {
			return fmt.Errorf("build %s: %w", dir, err)
		}
		fmt.Printf("built %s (%s, %d deps)\n", m.Filename(), m.Desc, len(m.Deps))
	}
	return nil
}

func cmdRepoAdd(dir string) error {
	entries, err := repo.Build(dir)
	if err != nil {
		return err
	}
	fmt.Printf("indexed %d packages in %s\n", len(entries), dir)
	return nil
}

func cmdInstall(vals map[string]string, names []string) error {
	if len(names) == 0 {
		return fmt.Errorf("install: need at least one package name")
	}
	c := core.New(config(vals))
	var done []string
	err := animate("installing "+strings.Join(names, ", "), func() (err error) {
		done, err = c.Install(names...)
		return err
	})
	if err != nil {
		return err
	}
	for _, n := range done {
		fmt.Printf("installed %s\n", n)
	}
	return nil
}

func cmdRemove(vals map[string]string, names []string) error {
	if len(names) == 0 {
		return fmt.Errorf("remove: need at least one package name")
	}
	c := core.New(config(vals))
	if err := animate("removing "+strings.Join(names, ", "), func() error { return c.Remove(names...) }); err != nil {
		return err
	}
	for _, n := range names {
		fmt.Printf("removed %s\n", n)
	}
	return nil
}

func cmdUpgrade(vals map[string]string) error {
	c := core.New(config(vals))
	var done []string
	err := animate("checking for upgrades", func() (err error) {
		done, err = c.Upgrade()
		return err
	})
	if err != nil {
		return err
	}
	if len(done) == 0 {
		fmt.Println("nothing to upgrade")
		return nil
	}
	for _, n := range done {
		fmt.Printf("upgraded %s\n", n)
	}
	return nil
}

func cmdSearch(vals map[string]string, args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("search: need a term")
	}
	c := core.New(config(vals))
	entries, err := c.Search(args[0])
	if err != nil {
		return err
	}
	if len(entries) == 0 {
		fmt.Println("no matches")
		return nil
	}
	for _, e := range entries {
		fmt.Printf("%s %s  %s\n", e.Meta.Name, e.Meta.Version, e.Meta.Desc)
	}
	return nil
}

func cmdList(vals map[string]string) error {
	c := core.New(config(vals))
	installed, err := c.Installed()
	if err != nil {
		return err
	}
	if len(installed) == 0 {
		fmt.Println("nothing installed")
		return nil
	}
	for _, m := range installed {
		fmt.Printf("%s %s  %s\n", m.Name, m.Version, m.Desc)
	}
	return nil
}
