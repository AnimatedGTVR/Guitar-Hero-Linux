package tui

import (
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

const (
	titleText = "ampkg — GHL package manager"
	helpText  = "↑/↓ navigate · enter select · q/ctrl+c quit"
)

var (
	titleStyle    = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("39")).Padding(0, 1)
	selectedStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("39")).Background(lipgloss.Color("236")).Padding(0, 1)
	itemStyle     = lipgloss.NewStyle().Padding(0, 1)
	descStyle     = lipgloss.NewStyle().Faint(true).Padding(0, 1)
	statusStyle   = lipgloss.NewStyle().Faint(true).MarginTop(1)
	helpStyle     = lipgloss.NewStyle().Faint(true).MarginTop(1)
)

type menuItem struct {
	label string
	desc  string
}

var menuItems = []menuItem{
	{label: "Install", desc: "Install packages from a GHL repo"},
	{label: "Remove", desc: "Remove installed packages"},
	{label: "Search", desc: "Search packages in the configured repos"},
	{label: "Update", desc: "Update the package database"},
	{label: "Upgrade", desc: "Upgrade all installed packages"},
	{label: "Quit", desc: "Exit ampkg"},
}

type model struct {
	cursor int
	status string
}

func initialModel() model {
	return model{}
}

func (m model) Init() tea.Cmd {
	return nil
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
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
			if m.cursor < len(menuItems)-1 {
				m.cursor++
			}
		case "enter", " ":
			item := menuItems[m.cursor]
			if item.label == "Quit" {
				return m, tea.Quit
			}
			m.status = fmt.Sprintf("%s: coming soon", item.label)
		}
	}
	return m, nil
}

func (m model) View() string {
	var b strings.Builder

	b.WriteString(titleStyle.Render(titleText))
	b.WriteString("\n\n")

	for i, item := range menuItems {
		label := itemStyle.Render(item.label)
		desc := descStyle.Render("· " + item.desc)
		if i == m.cursor {
			label = selectedStyle.Render(item.label)
			desc = descStyle.Foreground(lipgloss.Color("39")).Render("· " + item.desc)
		}
		b.WriteString(label)
		b.WriteString("  ")
		b.WriteString(desc)
		b.WriteString("\n")
	}

	if m.status != "" {
		b.WriteString(statusStyle.Render(m.status))
	}

	b.WriteString(helpStyle.Render(helpText))
	return b.String()
}

// Run starts the ampkg TUI.
func Run() error {
	p := tea.NewProgram(initialModel(), tea.WithAltScreen())
	_, err := p.Run()
	return err
}
