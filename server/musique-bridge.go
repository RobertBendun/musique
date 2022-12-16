package main

import (
	"C"
	"fmt"
	"log"
	"musique/server/router"
	"os"
	"sort"
)

//export ServerInit
func ServerInit(inputNick string, inputPort int) {
	nick = inputNick
	port = inputPort

	r := router.Router{}
	registerRoutes(&r)
	_, err := r.Run(baseIP, uint16(port))
	if err != nil {
		log.Fatalln(err)
	}

	if err := registerRemotes(); err != nil {
		log.Fatalln(err)
	}
}

//export ServerBeginProtocol
func ServerBeginProtocol() {
}

//export ListKnownRemotes
func ListKnownRemotes() {
	type nickAddr struct {
		nick, addr string
	}

	list := []nickAddr{}
	for _, remote := range remotes {
		list = append(list, nickAddr { remote.Nick, remote.Address})
	}

	sort.Slice(list, func (i, j int) bool {
		if list[i].nick == list[j].nick {
			return list[i].addr < list[j].addr
		}
		return list[i].nick < list[j].nick
	})

	for _, nickAddr := range list {
		fmt.Printf("%s@%s\n", nickAddr.nick, nickAddr.addr)
	}
	os.Stdout.Sync()
}
