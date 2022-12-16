package proto

type StartResponse struct {
	Succeded bool
}

func Start(startTime int64) (req Request) {
	req.Type = "start"
	req.Version = Version
	req.StartTime = startTime
	return
}
