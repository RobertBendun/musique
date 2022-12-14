package proto

import (
	"encoding/json"
	"net"
	"time"
)

func Command(target string, request interface{}, response interface{}) error {
	conn, err := net.Dial("tcp", target)
	if err != nil {
		return err
	}
	defer conn.Close()

	if err = json.NewEncoder(conn).Encode(request); err != nil {
		return err
	}

	return json.NewDecoder(conn).Decode(response)
}

func CommandTimeout(target string, request interface{}, response interface{}, timeout time.Duration) error {
	conn, err := net.DialTimeout("tcp", target, timeout)
	if err != nil {
		return err
	}
	defer conn.Close()

	if err = json.NewEncoder(conn).Encode(request); err != nil {
		return err
	}

	return json.NewDecoder(conn).Decode(response)
}
