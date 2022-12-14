# Server

## Development notes


### Testing server list synchronisation (2022-12-14)

For ease of testing you can launch N instances of server in N tmux panes.

```console
$ while true; do sleep {n}; echo "======="; ./server -nick {nick} -port {port}; done
```

where `n` is increasing for each server to ensure that initial scan would not cover entire task of network scanning.

Next you can use this script to test:

```bash
go build
killall server
sleep 4

# Repeat line below for all N servers
echo '{"version":1, "type":"hosts"}' | nc localhost {port} | jq

# Choose one or few that will request synchronization with their remotes
echo '{"version":1, "type":"synchronize-hosts-with-remotes"}' | nc localhost {port} | jq

# Ensure that all synchronisation propagated with enough sleep time
sleep 2

# Repeat line below for all N servers
echo '{"version":1, "type":"hosts"}' | nc localhost {port} | jq

```
