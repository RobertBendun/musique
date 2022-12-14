package proto

type HandshakeResponse struct {
	Nick    string
	Version string
}

func Handshake() (req Request) {
	req.Type = "handshake"
	req.Version = Version
	return
}
