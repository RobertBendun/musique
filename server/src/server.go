package main

import (
	"bufio"
	"log"
	"net"
	"os"
)

func main() {

	// HTTP part
	// fileServer := http.FileServer(http.Dir("./static"))
	// http.Handle("/", fileServer)
	// http.HandleFunc("/cmd", cmdHandler)
	// http.HandleFunc("/scan", scanHandler)

	// http.HandleFunc("/hello", helloHandler)

	// TCP part
	l, err := net.Listen("tcp", ":8081")
	if err != nil {
		log.Fatal(err)
	}
	defer l.Close()
	for {
		conn, err := l.Accept()
		if err != nil {
			log.Fatal(err)
		}
		go func(c net.Conn) {
			s := bufio.NewScanner(c)
			conn.Write([]byte("> "))
			for s.Scan() {
				resp := s.Text()
				if resp == "scan" {
					conn.Write([]byte("Scanning...\n"))
					conn.Write([]byte(scan() + "\n"))
					continue
				}
				if resp == "time" {
					conn.Write([]byte(showTime().String() + "\n"))
				}
				if resp == "timesync" {
					conn.Write([]byte(showMonoTime().String() + "\n"))
				}
				if resp == "quit" {
					c.Close()
					os.Exit(0)
				}
				conn.Write([]byte("> "))
			}
			c.Close()
		}(conn)
	}
}
