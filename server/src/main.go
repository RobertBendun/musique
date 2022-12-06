package main

import (
	"bufio"
	"fmt"
	"log"
	"net"
	"os"
	"time"
)

func scanError(scanResult []string, conn net.Conn) {
	if scanResult == nil {
		conn.Write([]byte("Empty scan result, run 'scan' first.\n"))
	}
}

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
			var scanResult []string
			for s.Scan() {
				resp := s.Text()
				if resp == "scan" {
					conn.Write([]byte("Scanning...\n"))
					scanResult = scan()
					conn.Write([]byte("Scanning done!\n"))
					conn.Write([]byte("> "))
					fmt.Println(len(scanResult))
					continue
				}
				if resp == "time" {
					conn.Write([]byte(showTime().String() + "\n"))
					conn.Write([]byte("> "))
					continue
				}
				if resp == "hosts" {
					scanError(scanResult, conn)
					for _, host := range scanResult {
						conn.Write([]byte(host + "\n"))
						fmt.Println("CONNECTED")
					}
					conn.Write([]byte("> "))
					continue
				}
				if resp == "showtime" {
					cTime := showTime()
					conn.Write([]byte(cTime.String() + "\n"))
				}

				if resp == "timesync" {
					time.Sleep(1 * time.Second)
					conn.Write([]byte("Server time: " + time.Now().String() + "\n"))
					scanError(scanResult, conn)
					for _, host := range scanResult {
						if host == "" {
							fmt.Print("No host")
						}
						fmt.Println(host)
						conn.Write([]byte("Waiting for time from " + host + "\n"))
						outconn, err := net.Dial("tcp", host)
						if err != nil {
							fmt.Println(err)
							os.Exit(1)
						}
						defer outconn.Close()
						recvBuf := make([]byte, 1024)
						msg := []byte("showtime\n")
						n, err2 := outconn.Write(msg)
						if n != len(msg) {
							fmt.Println("didn't send all the bytes")
							os.Exit(1)
						}
						if err2 != nil {
							fmt.Println(err)
							os.Exit(1)
						}
						// Temporary fix, fixme pls
						var read int
						for {
							var err3 error
							read, err3 = outconn.Read(recvBuf)
							if read > 2 {
								break
							}
							if err3 != nil {
								fmt.Println(err)
								os.Exit(1)
							}
						}
						_, err4 := conn.Write(recvBuf)
						if err4 != nil {
							fmt.Println(err)
							os.Exit(1)
						}
						outconn.Close()
					}
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
