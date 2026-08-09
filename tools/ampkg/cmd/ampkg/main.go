package main

import (
	"log"

	"ghl/ampkg/internal/tui"
)

func main() {
	if err := tui.Run(); err != nil {
		log.Fatal(err)
	}
}
