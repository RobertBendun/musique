package main

import (
	"fmt"
	"net"
	"time"
)

func scan() string {
	information := ""
	ifaces, _ := net.Interfaces()
	for _, i := range ifaces {
		addrs, _ := i.Addrs()
		for _, j := range addrs {
			ipv4, _, _ := net.ParseCIDR(j.String())
			if ipv4.IsGlobalUnicast() && ipv4.To4() != nil {
				ipv4 = ipv4.To4()
				ipv4 = ipv4.Mask(ipv4.DefaultMask())
				ipv4[3]++
				for i := 1; i < 256; i++ {
					_, dialErr := net.DialTimeout("tcp", ipv4.String()+":8081", time.Duration(1)*time.Second)
					if dialErr != nil {
						fmt.Println("Cannot connect to " + ipv4.String())
					} else {
						fmt.Println("Response from " + ipv4.String())
						information = fmt.Sprintf("%s\n%s", information, ipv4)
					}
					ipv4[3]++
				}
			}
		}
	}
	return information

}
