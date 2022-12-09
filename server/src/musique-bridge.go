package main

import (
	"C"
	"net"
	"bufio"
	"fmt"
	"time"
	"strings"
)

var clients []client
var pinger chan struct{}

//export ServerInit
func ServerInit() {
	// scanResult = scan()
	scanResult := []string{
		"10.100.5.112:8081",
		"10.100.5.44:8081",
	}
	clients = timesync(scanResult)

	pinger = make(chan struct{}, 100)

	go func() {
		l, err := net.Listen("tcp", ":8081")
		if err != nil {
			return
		}
		defer l.Close()
		for {
			conn, err := l.Accept()
			if err != nil {
				continue
			}
			go func(c net.Conn) {
				defer c.Close()
				s := bufio.NewScanner(c)
				for s.Scan() {
					response := s.Text()
					if response == "time" {
						fmt.Fprintln(conn, time.Now().UnixMilli())
						continue
					}

					if strings.HasPrefix(response, "start") {
						startTimeString := strings.TrimSpace(response[len("start"):])
						startTime := int64(0)
						fmt.Sscanf(startTimeString, "%d", &startTime)
						time.Sleep(time.Duration(startTime) * time.Millisecond)
						pinger <- struct{}{}
						continue
					}
				}
			}(conn)
		}
	}()
}

//export ServerBeginProtocol
func ServerBeginProtocol() {
	self := notifyAll(clients)
	select {
	case <- self:
	case <- pinger:
	}
}
