package main

import (
	"fmt"
	"net"
	"sync"
	"time"
)

func scan() []string {
	var wg sync.WaitGroup
	ips := make(chan string, 256)

	ifaces, _ := net.Interfaces()
	for _, iface := range ifaces {
		addrs, _ := iface.Addrs()
		for _, addr := range addrs {
			ipv4, _, _ := net.ParseCIDR(addr.String())

			if ipv4.IsGlobalUnicast() && ipv4.To4() != nil {
				ipv4 = ipv4.To4()
				ipv4 = ipv4.Mask(ipv4.DefaultMask())

				for i := 1; i < 255; i++ {
					localIP := make([]byte, 4)
					copy(localIP, ipv4)
					wg.Add(1)
					go func(ip net.IP) {
						conn, dialErr := net.DialTimeout("tcp", ip.String()+":8081", time.Duration(1)*time.Second)
						if dialErr == nil {
							ips <- ip.String() + ":8081"
							conn.Close()
						}
						wg.Done()
					}(localIP)
					ipv4[3]++
				}
			}
		}
	}

	go func() {
		wg.Wait()
		close(ips)
	}()

	information := []string{}
	for ip := range ips {
		fmt.Println("Response from " + ip)
		information = append(information, ip)
	}
	return information
}
