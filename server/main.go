package main

import (
	"bufio"
	"errors"
	"flag"
	"fmt"
	"log"
	"musique/server/proto"
	"musique/server/scan"
	"musique/server/router"
	"net"
	"os"
	"strings"
	"sync"
	"time"
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
		log.Printf("estimateFor: %v\n", err)
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
			exchange := timeExchange{}
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

func isThisMyAddress(address string) bool {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return false
	}

	for _, addr := range addrs {
		ip, _, err := net.ParseCIDR(addr.String())
		if err == nil && ip.To4() != nil && fmt.Sprintf("%s:%d", ip, port) == address {
			return true
		}
	}

	return false
}

func notifyAll(clients []client) <-chan time.Time {
	wg := sync.WaitGroup{}
	wg.Add(len(clients))
	startDeadline := time.After(maxReactionTime * time.Millisecond)

	for _, client := range clients {
		client := client
		go func() {
			startTime := maxReactionTime - (client.after-client.before)/2
			_, err := remotef(client.addr, fmt.Sprintf("start %d", startTime), "")
			if err != nil {
				log.Printf("failed to notify %s: %v\n", client.addr, err)
			}
			wg.Done()
		}()
	}

	return startDeadline
}

func registerRoutes(r *router.Router) {
	r.Add("handshake", func(incoming net.Conn, request proto.Request) interface{} {
		var response proto.HandshakeResponse
		response.Version = proto.Version
		response.Nick = nick
		return response
	})

	r.Add("hosts", func(incoming net.Conn, request proto.Request) interface{} {
		var response proto.HostsResponse
		for _, remote := range remotes {
			response.Hosts = append(response.Hosts, proto.HostsResponseEntry{
				Nick:    remote.Nick,
				Version: remote.Version,
				Address: remote.Address,
			})
		}
		return response
	})

	r.Add("synchronize-hosts", func(incoming net.Conn, request proto.Request) interface{} {
		return synchronizeHosts(request.HostsResponse)
	})


	r.Add("synchronize-hosts-with-remotes", func(incoming net.Conn, request proto.Request) interface{} {
		synchronizeHostsWithRemotes()
		return nil
	})
}

type Remote struct {
	Address string
	Nick    string
	Version string
}

var (
	baseIP  string = ""
	nick    string
	port    int = 8888
	remotes map[string]Remote
)

func synchronizeHosts(incoming proto.HostsResponse) (response proto.HostsResponse) {
	visitedHosts := make(map[string]struct{})

	// Add all hosts that are in incoming to our list of remotes
	// Additionaly build set of all hosts that remote knows
	for _, incomingHost := range incoming.Hosts {
		if _, ok := remotes[incomingHost.Address]; !ok && !isThisMyAddress(incomingHost.Address) {
			remotes[incomingHost.Address] = Remote{
				Address: incomingHost.Address,
				Nick:    incomingHost.Nick,
				Version: incomingHost.Version,
			}
		}
		visitedHosts[incomingHost.Address] = struct{}{}
	}

	// Build list of hosts that incoming doesn't know
	for _, remote := range remotes {
		if _, ok := visitedHosts[remote.Address]; !ok {
			response.Hosts = append(response.Hosts, proto.HostsResponseEntry{
				Address: remote.Address,
				Version: remote.Version,
				Nick:    remote.Nick,
			})
		}
	}
	return
}

func myAddressInTheSameNetwork(remote string) (string, error) {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "", fmt.Errorf("myAddressInTheSameNetwork: %v", err)
	}

	remoteInParts := strings.Split(remote, ":")
	if len(remoteInParts) == 2 {
		remote = remoteInParts[0]
	}

	remoteIP := net.ParseIP(remote)
	if remoteIP == nil {
		// TODO Hoist error to global variable
		return "", errors.New("Cannot parse remote IP")
	}

	for _, addr := range addrs {
		ip, ipNet, err := net.ParseCIDR(addr.String())
		if err == nil && ipNet.Contains(remoteIP) {
			return ip.String(), nil
		}
	}

	// TODO Hoist error to global variable
	return "", errors.New("Cannot find matching IP addr")
}

func synchronizeHostsWithRemotes() {
	previousResponseLength := -1
	var response proto.HostsResponse

	for previousResponseLength != len(response.Hosts) {
		response = proto.HostsResponse{}

		// Add all known remotes
		for _, remote := range remotes {
			response.Hosts = append(response.Hosts, proto.HostsResponseEntry{
				Address: remote.Address,
				Nick:    remote.Nick,
				Version: remote.Version,
			})
		}

		// Send constructed list to each remote
		previousResponseLength = len(response.Hosts)
		for _, remote := range response.Hosts {
			var localResponse proto.HostsResponse
			localResponse.Hosts = make([]proto.HostsResponseEntry, len(response.Hosts))
			copy(localResponse.Hosts, response.Hosts)

			myAddress, err := myAddressInTheSameNetwork(remote.Address)
			// TODO Report when err != nil
			if err == nil {
				localResponse.Hosts = append(localResponse.Hosts, proto.HostsResponseEntry{
					Address: myAddress,
					Nick:    nick,
					Version: proto.Version,
				})
			}

			var remoteResponse proto.HostsResponse
			proto.Command(remote.Address, proto.SynchronizeHosts(localResponse), &remoteResponse)
			synchronizeHosts(remoteResponse)
		}
	}
}

func registerRemotes() error {
	networks, err := scan.AvailableNetworks()
	if err != nil {
		return err
	}

	hosts := scan.TCPHosts(networks, []uint16{8081, 8082, 8083, 8084})

	remotes = make(map[string]Remote)
	for host := range hosts {
		if !isThisMyAddress(host.Address) {
			remotes[host.Address] = Remote{
				Address: host.Address,
				Nick:    host.Nick,
				Version: host.Version,
			}
		}
	}

	return nil
}

func main() {
	var (
		logsPath string
	)

	flag.StringVar(&baseIP, "ip", "", "IP where server will listen")
	flag.StringVar(&nick, "nick", "", "Name that is going to be used to recognize this server")
	flag.IntVar(&port, "port", 8081, "TCP port where server receives connections")
	flag.StringVar(&logsPath, "logs", "", "Target file for logs from server. By default stdout")
	flag.Parse()

	if len(logsPath) != 0 {
		// TODO Is defer logFile.Close() needed here? Dunno
		logFile, err := os.OpenFile(logsPath, os.O_WRONLY|os.O_APPEND, 0o640)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Cannot open log file: %v\n", err)
			os.Exit(1)
		}
		log.SetOutput(logFile)
	}

	if len(nick) == 0 {
		log.Fatalln("Please provide nick via --nick flag")
	}

	r := router.Router{}
	registerRoutes(&r)
	exit, err := r.Run(baseIP, uint16(port))
	if err != nil {
		log.Fatalln(err)
	}

	if err := registerRemotes(); err != nil {
		log.Fatalln(err)
	}

	for range exit {
	}
}

func main2() {
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
			scanResult := []string{
				"10.100.5.112:8081",
				"10.100.5.44:8081",
			}
			var clients []client
			for s.Scan() {
				resp := s.Text()
				log.Println(resp)
				if resp == "scan" {
					conn.Write([]byte("Scanning...\n"))
					scanResult = nil // scan()
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
					<-notifyAll(clients)
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
