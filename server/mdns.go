package main

import (
	"context"
	"fmt"
	"log"
	"musique/server/proto"
	"net"
	"sync"
	"time"

	"github.com/RobertBendun/zeroconf/v2"
)

func doHandshake(wg *sync.WaitGroup, service *zeroconf.ServiceEntry, remotes chan<- Remote, timeout time.Duration) {
	for _, ip := range service.AddrIPv4 {
		wg.Add(1)
		go func(ip net.IP) {
			defer wg.Done()
			target := fmt.Sprintf("%s:%d", ip, service.Port)
			if isThisMyAddress(target) {
				return
			}

			var hs proto.HandshakeResponse
			err := proto.CommandTimeout(target, proto.Handshake(), &hs, timeout)
			if err == nil {
				log.Println("Received handshake response", target, hs)
				remotes <- Remote{
					Address: target,
					Nick:    hs.Nick,
					Version: hs.Version,
				}
			}
		}(ip)
	}
}

// Register all remotes that cane be found in `waitTime` seconds
func registerRemotes(waitTime int) error {
	wg := sync.WaitGroup{}
	done := make(chan error, 1)
	incomingRemotes := make(chan Remote, 32)
	entries := make(chan *zeroconf.ServiceEntry, 12)
	timeout := time.Second*time.Duration(waitTime)

	wg.Add(1)
	go func(results <-chan *zeroconf.ServiceEntry) {
		for entry := range results {
			log.Println("Found service at", entry.HostName, "at addrs", entry.AddrIPv4)
			doHandshake(&wg, entry, incomingRemotes, timeout)
		}
		wg.Done()
		done <- nil
	}(entries)

	go func() {
		wg.Wait()
		close(incomingRemotes)
	}()

	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	go func() {
		err := zeroconf.Browse(ctx, "_musique._tcp", "local", entries)
		if err != nil {
			done <- fmt.Errorf("failed to browse: %v", err)
		}
		<-ctx.Done()
	}()

	remotes = make(map[string]*Remote)
	for remote := range incomingRemotes {
		remote := remote
		remotes[remote.Address] = &remote
	}

	return <-done
}

type dnsServer interface {
	Shutdown()
}

func registerDNS() (dnsServer, error) {
	return zeroconf.Register("Musique", "_musique._tcp", "local", port, []string{}, nil)
}
