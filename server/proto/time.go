package proto

type TimeResponse struct {
	Time int64
}

func Time() (req Request) {
	req.Type = "time"
	req.Version = Version
	return
}
