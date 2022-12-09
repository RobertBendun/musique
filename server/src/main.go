package main

import (
	"bufio"
	"fmt"
	"log"
	"net"
	"os"
	"sync"
	"time"
	"strings"
)

func scanError(scanResult []string, conn net.Conn) {
	if scanResult == nil {
		conn.Write([]byte("Empty scan result, run 'scan' first.\n"))
	}
}

type timeExchange struct {
	before, after, remote int64
}

type client struct {
	timeExchange
	id   int
	addr string
}

func remotef(host, command, format string, args ...interface{}) (int, error) {
	connection, err := net.Dial("tcp", host)
	if err != nil {
		return 0, fmt.Errorf("remotef: establishing connection: %v", err)
	}
	defer connection.Close()
	connection.SetDeadline(time.Now().Add(1 * time.Second))

	fmt.Fprintln(connection, command)
	if len(format) > 0 {
		parsedCount, err := fmt.Fscanf(connection, format, args...)
		if err != nil {
			return 0, fmt.Errorf("remotef: parsing: %v", err)
		}
	return parsedCount, nil
	}

	return 0, nil
}

func (e *timeExchange) estimateFor(host string) bool {
	e.before = time.Now().UnixMilli()
	parsedCount, err := remotef(host, "time", "%d\n", &e.remote)
	e.after = time.Now().UnixMilli()

	if err != nil {
		log.Println("estimateFor: %v", err)
		return false
	}
	if parsedCount != 1 {
		log.Printf("berkeley: expected to parse number, instead parsed %d items\n", parsedCount)
		return false
	}
	return true
}

func timesync(hosts []string) []client {
	wg := sync.WaitGroup{}
	wg.Add(len(hosts))

	// Gather time from each host
	responses := make(chan client, len(hosts))
	for id, host := range hosts {
		id, host := id, host
		go func() {
			defer wg.Done()
			exchange := timeExchange{};
			if exchange.estimateFor(host) {
				responses <- client{exchange, id, host}
			}
		}()
	}

	wg.Wait()
	close(responses)

	clients := make([]client, 0, len(hosts))
	for client := range responses {
		clients = append(clients, client)
	}

	fmt.Printf("Successfully gathered %d clients\n", len(clients))

	return clients
}

const maxReactionTime = 300

func notifyAll(clients []client) {
	wg := sync.WaitGroup{}
	wg.Add(len(clients))
	startDeadline := time.After(maxReactionTime * time.Millisecond)

	for _, client := range clients {
		client := client
		go func() {
			startTime := maxReactionTime - (client.after - client.before) / 2
			_, err := remotef(client.addr, fmt.Sprintf("start %d", startTime), "")
			if err != nil {
				log.Printf("failed to notify %s: %v\n", client.addr, err)
			}
			wg.Done()
		}()
	}

	<-startDeadline
	return
}

func main() {
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
			var scanResult []string
			var clients []client
			for s.Scan() {
				resp := s.Text()
				log.Println(resp)
				if resp == "scan" {
					conn.Write([]byte("Scanning...\n"))
					scanResult = scan()
					conn.Write([]byte("Scanning done!\n"))
					fmt.Println(len(scanResult))
					continue
				}
				if resp == "time" {
					fmt.Fprintln(conn, time.Now().UnixMilli())
					continue
				}
				if resp == "hosts" {
					scanError(scanResult, conn)
					for _, host := range scanResult {
						conn.Write([]byte(host + "\n"))
						fmt.Println("CONNECTED")
					}
					continue
				}
				if resp == "showtime" {
					cTime := showTime()
					conn.Write([]byte(cTime.String() + "\n"))
					continue
				}
				if resp == "timesync" {
					clients = timesync(scanResult)
					continue
				}
				if strings.HasPrefix(resp, "start") {
					startTimeString := strings.TrimSpace(resp[len("start"):])
					startTime := int64(0)
					fmt.Sscanf(startTimeString, "%d", &startTime)
					time.Sleep(time.Duration(startTime) * time.Millisecond)
					log.Println("Started #start")
					continue
				}
				if resp == "notify" {
					notifyAll(clients)
					log.Println("Started #notify")
					continue
				}
				if resp == "quit" {
					c.Close()
					os.Exit(0)
				}
			}
			c.Close()
		}(conn)
	}
}
