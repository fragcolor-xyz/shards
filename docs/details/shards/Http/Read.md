The output of this shard is a table with the following format:
```shards
{
"method": "POST",
"headers": {
  "Content-Type": "application/json",
  "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
  "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "Accept": "application/json",
  "Content-Length": "62"
},
"target": "/api/users",
  "body": "{\"username\":\"johndoe\",\"email\":\"john@example.com\",\"age\":30}"
}
```

Each key in the table represent a different aspect of the request.
**"method"** (string):
- This represents the HTTP method used in the request.
- Examples: "GET", "POST", "PUT", "DELETE", "HEAD"
- It indicates the desired action to be performed on the resource.
**"headers"** (string table):
- This is a collection of HTTP headers sent with the request.
- Each key-value pair in this table represents a header name and its value.
- Examples might include:
  - "Content-Type": "application/json"
  - "User-Agent": "Mozilla/5.0 ..."
  - "Authorization": "Bearer token123"
**"target"** (string):
- This is the request target, which is typically the path part of the URL.
- For example, in "http://example.com/api/users", the target would be "/api/users".
- It identifies the resource on the server that the request is directed to.
**"body"** (string):
- This contains the payload of the request, if any.
- For DELETE requests, this is typically empty.
- For POST or PUT requests, this might contain data being sent to the server, like form data or JSON.
