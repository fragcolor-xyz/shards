# SSH Test Server

Simple SSH server for testing the SSH shards module.

## Usage

Start the server:
```bash
cd shards/tests/ssh-test-docker
docker-compose up -d
```

Stop the server:
```bash
docker-compose down
```

## Connection Details

- **Host**: localhost
- **Port**: 2222
- **Username**: testuser
- **Password**: test123

## Test Example

```shards
(def host "localhost")
(def port 2222)
(def user "testuser")
(def password "test123")

(SSH.Connect
 :Host host
 :Port port
 :User user
 :Password password) >= ssh-session

("ls -la" | SSH.Execute :Session ssh-session | Log)
```
