package tui

import (
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"

	"ghl/ampkg/internal/core"
)

const helpText = "↑/↓ navigate · enter select · q/ctrl+c quit"

var (
	titleStyle    = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("39")).Padding(0, 1)
	selectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("39")).Background(lipgloss.Color("236")).Padding(0, 1)
	itemStyle     = lipgloss.NewStyle().Padding(0, 1)
	descStyle     = lipgloss.NewStyle().Faint(true).Padding(0, 1)
	statusStyle   = lipgloss.NewStyle().Faint(true).MarginTop(1)
	helpStyle     = lipgloss.NewStyle().Faint(true).MarginTop(1)
	errorStyle    = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("196")).MarginTop(1)
)

type menuItem struct {
	label string
	desc  string
	run   func(*core.Config) tea.Model
}

type menuModel struct {
	cfg    *core.Config
	items  []menuItem
	cursor int
	status string
}

func newMenuModel(cfg *core.Config) menuModel {
	return menuModel{
		cfg: cfg,
		items: []menuItem{
			{label: "Install", desc: "Install packages from a GHL repo", run: installScreen},
			{label: "Remove", desc: "Remove installed packages", run: removeScreen},
			{label: "Search", desc: "Search packages in the configured repos", run: searchScreen},
			{label: "Update", desc: "Update the package database", run: func(*core.Config) tea.Model { return stubScreen("Update", "not implemented yet") }},
			{label: "Upgrade", desc: "Upgrade all installed packages", run: func(*core.Config) tea.Model { return stubScreen("Upgrade", "not implemented yet") }},
			{label: "Quit", desc: "Exit ampkg", run: nil},
		},
	}
}

func (m menuModel) Init() tea.Cmd { return nil }

func (m menuModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q":
			return m, tea.Quit
		case "up", "k":
			if m.cursor > 0 {
				m.cursor--
			}
		case "down", "j":
			if m.cursor < len(m.items)-1 {
				m.cursor++
			}
		case "enter", " ":
			item := m.items[m.cursor]
			if item.run == nil {
				return m, tea.Quit
			}
			return item.run(m.cfg), nil
		}
	}
	return m, nil
}

func (m menuModel) View() string {
	var b strings.Builder
	b.WriteString(titleStyle.Render("ampkg — GHL package manager"))
	b.WriteString("\n\n")
	for i, item := range m.items {
		label := itemStyle.Render(item.label)
		desc := descStyle.Render("· " + item.desc)
		if i == m.cursor {
			label = selectedStyle.Render(item.label)
			desc = descStyle.Foreground(lipgloss.Color("39")).Render("· " + item.desc)
		}
		b.WriteString(label + "  " + desc + "\n")
	}
	if m.status != "" {
		b.WriteString(statusStyle.Render(m.status))
	}
	b.WriteString(helpStyle.Render(helpText))
	return b.String()
}

// stubScreen is a placeholder for features that aren't real yet.
func stubScreen(title, status string) tea.Model {
	return messageModel{title: title, status: status, back: newMenuModel(&core.Config{})}
}

// Run starts the ampkg TUI.
func Run(cfg *core.Config) error {
	p := tea.NewProgram(newMenuModel(cfg), tea.WithAltScreen())
	_, err := p.Run()
	return err
}

// messageModel shows a title + status and returns to the menu on any key.
type messageModel struct {
	title  string
	status string
	back   tea.Model
}

func (m messageModel) Init() tea.Cmd { return nil }

func (m messageModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	if _, ok := msg.(tea.KeyMsg); ok {
		return m.back, nil
	}
	return m, nil
}

func (m messageModel) View() string {
	return fmt.Sprintf("%s\n\n%s\n\npress any key to go back", titleStyle.Render(m.title), m.status)
}
