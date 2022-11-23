# Musique server API draft

## Server should be able to make following requests:

### Send command to interpreter

Request:
```
POST /cmd/command
```

Response:
```
{}
```

### Fetch state of the interpreter

Request:
```
GET /snapshot
```

Response:
```
{
    "state": {
        "oct": 4,
        "bmp": 120,
        ...
    },
}
```

## Server also should handle following request:

### Synchronization of states

Request:
```
GET /sync
```

Response:
```
{}
```

### Template for more requests

Request:
```

```

Response:
```

```