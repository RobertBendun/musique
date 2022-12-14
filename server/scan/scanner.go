package scan

import (
	"fmt"
	"musique/server/proto"
	"net"
	"sync"
	"time"
)

type Network struct {
	FirstAddress  string
	MaxHostsCount int
}

const timeoutTCPHosts = time.Duration(1) * time.Second

func nextIP(ip net.IP) net.IP {
	// FIXME Proper next IP address in network calculation
	next := make([]byte, 4)
	copy(next, ip)
	next[3]++
	return next
}

// AvailableNetworks returns all IPv4 networks that are available to the host
func AvailableNetworks() ([]Network, error) {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return nil, fmt.Errorf("getting interfaces info: %v", err)
	}

	networks := []Network{}

	for _, addr := range addrs {
		_, ipNet, err := net.ParseCIDR(addr.String())
		if err != nil {
			continue
		}

		if ipNet.IP.IsGlobalUnicast() && ipNet.IP.To4() != nil {
			// FIXME We assume mask /24. This is a reasonable assumption performance wise
			// but may lead to inability to recognize some of the host in network
			networks = append(networks, Network{nextIP(ipNet.IP).String(), 254})
		}
	}

	return networks, nil
}

type Response struct {
	proto.HandshakeResponse
	Address string
}

// TCPHosts returns all TCP hosts that are in given networks on one of given ports
func TCPHosts(networks []Network, ports []uint16) <-chan Response {
	ips := make(chan Response, 32)

	wg := sync.WaitGroup{}

	for _, network := range networks {
		ip := net.ParseIP(network.FirstAddress)
		for i := 0; i < network.MaxHostsCount; i++ {
			for _, port := range ports {
				wg.Add(1)
				go func(ip net.IP, port uint16) {
					defer wg.Done()
					target := fmt.Sprintf("%s:%d", ip, port)
					var hs proto.HandshakeResponse

					err := proto.CommandTimeout(target, proto.Handshake(), &hs, timeoutTCPHosts)
					if err == nil {
						ips <- Response{hs, target}
					}
				}(ip, port)
			}
			ip = nextIP(ip)
		}
	}

	go func() {
		wg.Wait()
		close(ips)
	}()

	return ips
}
