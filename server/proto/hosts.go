package proto

type HostsResponseEntry struct {
	Nick    string
	Address string
	Version string
}

type HostsResponse struct {
	Hosts []HostsResponseEntry
}

func Hosts() (req Request) {
	req.Version = Version
	req.Type = "hosts"
	return
}

func SynchronizeHosts(response HostsResponse) (req Request) {
	req.HostsResponse = response
	req.Version = Version
	req.Type = "synchronize-hosts"
	return
}
