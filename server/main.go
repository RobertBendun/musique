package main

import (
	"errors"
	"flag"
	"fmt"
	"log"
	"musique/server/proto"
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

type TimeExchange struct {
	before, after, remote int64
}

func (e *TimeExchange) estimateFor(host string) bool {
	e.before = time.Now().UnixMilli()
	var response proto.TimeResponse
	if err := proto.Command(host, proto.Time(), &response); err != nil {
		log.Printf("failed to send time command: %v\n", err)
		return false
	}
	e.after = time.Now().UnixMilli()
	return true
}

func timesync() {
	wg := sync.WaitGroup{}
	wg.Add(len(remotes))

	type response struct {
		TimeExchange
		key string
	}

	// Gather time from each host
	responses := make(chan response, len(remotes))
	for key, remote := range remotes {
		key, remote := key, remote
		go func() {
			defer wg.Done()
			exchange := TimeExchange{}
			if exchange.estimateFor(remote.Address) {
				responses <- response{exchange, key}
			}
		}()
	}

	wg.Wait()
	close(responses)

	for response := range responses {
		remote := remotes[response.key]
		remote.TimeExchange = response.TimeExchange
	}
}

var maxReactionTime = int64(1_000)

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

func notifyAll() <-chan time.Time {
	wg := sync.WaitGroup{}
	wg.Add(len(remotes))
	startDeadline := time.After(time.Duration(maxReactionTime) * time.Millisecond)

	for _, remote := range remotes {
		remote := remote
		go func() {
			startTime := maxReactionTime - (remote.after-remote.before)/2
			var response proto.StartResponse
			err := proto.CommandTimeout(
				remote.Address,
				proto.Start(startTime),
				&response,
				time.Duration(startTime)*time.Millisecond,
			)
			if err != nil {
				log.Printf("failed to notify %s: %v\n", remote.Address, err)
			}
			wg.Done()
		}()
	}

	return startDeadline
}

var (
	pinger chan struct{}
)

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

	r.Add("time", func(incoming net.Conn, request proto.Request) interface{} {
		return proto.TimeResponse{
			Time: time.Now().UnixMilli(),
		}
	})

	pinger = make(chan struct{}, 12)
	r.Add("start", func(incoming net.Conn, request proto.Request) interface{} {
		go func() {
			time.Sleep(time.Duration(request.StartTime) * time.Millisecond)
			pinger <- struct{}{}
		}()
		return proto.StartResponse{
			Succeded: true,
		}
	})
}

type Remote struct {
	TimeExchange
	Address string
	Nick    string
	Version string
}

var (
	baseIP  string = ""
	nick    string
	port    int = 8888
	remotes map[string]*Remote
)

func synchronizeHosts(incoming proto.HostsResponse) (response proto.HostsResponse) {
	visitedHosts := make(map[string]struct{})

	// Add all hosts that are in incoming to our list of remotes
	// Additionaly build set of all hosts that remote knows
	for _, incomingHost := range incoming.Hosts {
		if _, ok := remotes[incomingHost.Address]; !ok && !isThisMyAddress(incomingHost.Address) {
			remotes[incomingHost.Address] = &Remote{
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
			return fmt.Sprintf("%s:%d", ip, port), nil
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

	server, err := registerDNS()
	if err != nil {
		log.Fatalln("Failed to register DNS:", err)
	}
	defer server.Shutdown()

	r := router.Router{}
	registerRoutes(&r)
	exit, err := r.Run(baseIP, &port)
	if err != nil {
		log.Fatalln(err)
	}

	if err := registerRemotes(5); err != nil {
		log.Fatalln(err)
	}

	for range exit {
	}
}
