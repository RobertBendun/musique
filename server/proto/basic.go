package proto

const Version = "1"

type Request struct {
	Version string
	Type    string
	HostsResponse
}
