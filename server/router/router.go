package router

import (
	"encoding/json"
	"musique/server/proto"
	"net"
	"log"
	"fmt"
)

type Route func(incoming net.Conn, request proto.Request) interface{}

type Router struct {
	routes map[string]Route
	port int
	baseIP string
}

func (router *Router) Add(name string, route Route) {
	if router.routes == nil {
		router.routes = make(map[string]Route)
	}
	router.routes[name] = route
}

func (router *Router) Run(ip string, port *int) (<-chan struct{}, error) {
	router.port = *port
	router.baseIP = ip

	listener, err := net.Listen("tcp", fmt.Sprintf("%s:%d", router.baseIP, router.port))
	if err != nil {
		return nil, err
	}
	_, actualPort, err := net.SplitHostPort(listener.Addr().String())
	if err != nil {
		return nil, err
	}
	fmt.Sscan(actualPort, port)

	exit := make(chan struct{})

	go func() {
		defer listener.Close()
		defer close(exit)
		for {
			incoming, err := listener.Accept()
			if err != nil {
				log.Println("failed to accept connection:", err)
			}
			go router.handleIncoming(incoming)
		}
	}()

	return exit, nil
}

func (router *Router) handleIncoming(incoming net.Conn) {
	defer incoming.Close()

	request := proto.Request{}
	json.NewDecoder(incoming).Decode(&request)
	// log.Printf("%s: %+v\n", incoming.RemoteAddr(), request)

	if handler, ok := router.routes[request.Type]; ok {
		if response := handler(incoming, request); response != nil {
			json.NewEncoder(incoming).Encode(response)
		}
	} else {
		log.Printf("unrecongized route: %v\n", request.Type)
	}
}

