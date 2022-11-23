package main

import (
	"time"
)

func showTime() time.Time {
	time := time.Now()
	return time
}

func showMonoTime() time.Duration {
	start := time.Now()
	t := time.Now()
	elapsed := t.Sub(start)
	return elapsed
}
