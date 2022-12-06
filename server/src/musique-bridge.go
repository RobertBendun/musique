package main

import (
	"C"
	"fmt"
	"time"
)

//export ServerInit
func ServerInit() {
	fmt.Println("Initializing server")
}

//export ServerBeginProtocol
func ServerBeginProtocol() {
	protocol := []string{
		"Make the plan",
		"Execute the plan",
		"Expect the plan to go off the rails",
		"Throw away the plan",
	}

	for i, msg := range protocol {
		fmt.Printf("%d. %s\n", i, msg)
		time.Sleep(300 * time.Millisecond)
	}
}
