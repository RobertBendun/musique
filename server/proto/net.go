package proto

import (
	"encoding/json"
	"errors"
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
	responseChan := make(chan interface{})
	errorChan := make(chan error)

	go func() {
		conn, err := net.DialTimeout("tcp", target, timeout)
		if err != nil {
			errorChan <- err
			return
		}
		defer conn.Close()

		if err = json.NewEncoder(conn).Encode(request); err != nil {
			errorChan <- err
			return
		}

		responseChan <- json.NewDecoder(conn).Decode(response)
	}()

	select {
	case response = <-responseChan:
		return nil
	case err := <-errorChan:
		return err
	case <-time.After(timeout):
		return errors.New("timout")
	}
}
