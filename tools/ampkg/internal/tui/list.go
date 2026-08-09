package tui

import (
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"

	"ghl/ampkg/internal/core"
)

// listModel shows a selectable list of packages. Enter runs the action,
// refreshing the list afterwards; q/esc returns to the menu.
type listModel struct {
	cfg      *core.Config
	title    string
	lines    []string
	names    []string // package name for each line
	cursor   int
	status   string
	err      string
	refresh  func(*core.Config) ([]string, []string, error)
	action   func(*core.Config, string) error
	back     tea.Model
	helpHint string
}

func (m listModel) Init() tea.Cmd { return nil }

func (m listModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c":
			return m, tea.Quit
		case "q", "esc", "backspace":
			return m.back, nil
		case "up", "k":
			if m.cursor > 0 {
				m.cursor--
			}
		case "down", "j":
			if m.cursor < len(m.lines)-1 {
				m.cursor++
			}
		case "enter", " ":
			if len(m.names) == 0 {
				break
			}
			name := m.names[m.cursor]
			m.status = "working..."
			m.err = ""
			if err := m.action(m.cfg, name); err != nil {
				m.err = err.Error()
				m.status = ""
			} else {
				m.status = "done: " + name
				m.cursor = 0
				lines, names, err := m.refresh(m.cfg)
				if err != nil {
					m.err = err.Error()
				} else {
					m.lines, m.names = lines, names
				}
			}
		}
	}
	return m, nil
}

func (m listModel) View() string {
	var b strings.Builder
	b.WriteString(titleStyle.Render(m.title))
	b.WriteString("\n\n")
	if len(m.lines) == 0 {
		b.WriteString(statusStyle.Render("(nothing to show)"))
		b.WriteString("\n")
	}
	for i, line := range m.lines {
		if i == m.cursor {
			b.WriteString(selectedStyle.Render(line))
		} else {
			b.WriteString(itemStyle.Render(line))
		}
		b.WriteString("\n")
	}
	if m.status != "" {
		b.WriteString(statusStyle.Render(m.status))
	}
	if m.err != "" {
		b.WriteString(errorStyle.Render(m.err))
	}
	hint := m.helpHint
	if hint == "" {
		hint = helpText
	}
	b.WriteString(helpStyle.Render(hint))
	return b.String()
}

func installScreen(cfg *core.Config) tea.Model {
	return listModel{
		cfg:    cfg,
		title:  "Install — select a package",
		back:   newMenuModel(cfg),
		action: func(c *core.Config, name string) error { _, err := core.New(*c).Install(name); return err },
		refresh: func(c *core.Config) ([]string, []string, error) {
			entries, err := core.New(*c).Repo()
			if err != nil {
				return nil, nil, fmt.Errorf("no repo index at %s/index — run 'ampkg repo-add'", c.RepoDir)
			}
			var lines, names []string
			for _, e := range entries {
				lines = append(lines, fmt.Sprintf("%-20s %-14s %s", e.Meta.Name, e.Meta.Version, e.Meta.Desc))
				names = append(names, e.Meta.Name)
			}
			return lines, names, nil
		},
	}
}

func removeScreen(cfg *core.Config) tea.Model {
	return listModel{
		cfg:    cfg,
		title:  "Remove — select an installed package",
		back:   newMenuModel(cfg),
		action: func(c *core.Config, name string) error { return core.New(*c).Remove(name) },
		refresh: func(c *core.Config) ([]string, []string, error) {
			installed, err := core.New(*c).Installed()
			if err != nil {
				return nil, nil, err
			}
			var lines, names []string
			for _, m := range installed {
				lines = append(lines, fmt.Sprintf("%-20s %-14s %s", m.Name, m.Version, m.Desc))
				names = append(names, m.Name)
			}
			return lines, names, nil
		},
	}
}

// promptModel is a one-line text input.
type promptModel struct {
	cfg      *core.Config
	label    string
	input    string
	back     tea.Model
	onSubmit func(*core.Config, string) tea.Model
}

func (m promptModel) Init() tea.Cmd { return nil }

func (m promptModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c":
			return m, tea.Quit
		case "esc", "backspace":
			if m.input == "" {
				return m.back, nil
			}
			m.input = m.input[:len(m.input)-1]
		case "enter":
			return m.onSubmit(m.cfg, m.input), nil
		default:
			if msg.Type == tea.KeyRunes {
				m.input += msg.String()
			}
		}
	}
	return m, nil
}

func (m promptModel) View() string {
	return titleStyle.Render(m.label) + "\n\n  " + m.input + "▌\n\n" + helpStyle.Render("type to search · enter go · esc back")
}

func searchScreen(cfg *core.Config) tea.Model {
	return promptModel{
		cfg:   cfg,
		label: "Search — enter a term",
		back:  newMenuModel(cfg),
		onSubmit: func(c *core.Config, term string) tea.Model {
			results := listModel{
				cfg:    c,
				title:  "Search results for " + term,
				back:   newMenuModel(c),
				action: func(c *core.Config, name string) error { _, err := core.New(*c).Install(name); return err },
			}
			lines, names, err := func() ([]string, []string, error) {
				entries, err := core.New(*c).Search(term)
				if err != nil {
					return nil, nil, err
				}
				var lines, names []string
				for _, e := range entries {
					lines = append(lines, fmt.Sprintf("%-20s %-14s %s", e.Meta.Name, e.Meta.Version, e.Meta.Desc))
					names = append(names, e.Meta.Name)
				}
				return lines, names, nil
			}()
			results.lines, results.names = lines, names
			if err != nil {
				results.err = err.Error()
			}
			results.refresh = func(c *core.Config) ([]string, []string, error) { return lines, names, nil }
			return results
		},
	}
}

var _ = lipgloss.NewStyle
