package main

import (
	"C"
	"fmt"
	"log"
	"musique/server/router"
	"os"
	"sort"
)

const (
	initialWaitingTime         = 3
	userRequestedWatingingTime = 5
)

//export ServerInit
func ServerInit(inputNick string, inputPort int) {
	logFile, err := os.OpenFile("musique.log", os.O_WRONLY|os.O_APPEND|os.O_CREATE, 0o640)
	if err != nil {
		log.Fatalln(err)
	}
	log.SetOutput(logFile)

	nick = inputNick
	port = inputPort

	r := router.Router{}
	registerRoutes(&r)
	_, err = r.Run(baseIP, uint16(port))
	if err != nil {
		log.Fatalln(err)
	}

	_, err = registerDNS()
	if err != nil {
		log.Fatalln("Failed to register DNS:", err)
	}
	// defer server.Shutdown()

	if err := registerRemotes(initialWaitingTime); err != nil {
		log.Fatalln(err)
	}

	timesync()
}

//export SetTimeout
func SetTimeout(timeout int64) {
	maxReactionTime = timeout
}

//export ServerBeginProtocol
func ServerBeginProtocol() {
	self := notifyAll()
	select {
	case <-self:
	case <-pinger:
	}
}

//export Discover
func Discover() {
	if err := registerRemotes(userRequestedWatingingTime); err != nil {
		log.Println("discover:", err)
	}
	// synchronizeHostsWithRemotes()
}

//export ListKnownRemotes
func ListKnownRemotes() {
	type nickAddr struct {
		nick, addr string
	}

	list := []nickAddr{}
	for _, remote := range remotes {
		list = append(list, nickAddr{remote.Nick, remote.Address})
	}

	sort.Slice(list, func(i, j int) bool {
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
