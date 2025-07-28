A HTTP DELETE request is a method of sending a request to a server to delete a resource.

This request is idempotent. Multiple identical requests will have the same effect as a single request.

This shard can accept a string table, string or bytes as input and will modify the body of the DELETE request accordingly.
- **none** : no body is attached to the request
- **string** : The string is attached to the body as is and treated as a JSON string.
- **string table** : Both key and values need to be strings. The shard constructs a URL encoded string using the key and value pairs from the table and attaches it to the body of the request.
- **bytes** : The bytes are attached to the body as is.

If the output of the shard is a table, it will have the following format:
```shards
{
  status: Int,
  headers: Table(String, String),
  body: String or Bytes
}
```
- **status**: An integer representing the HTTP status code.
- **headers**: A table where both keys and values are strings, representing the response headers.
- **body**: Either a string or bytes (depending on the asBytes parameter), containing the response body.

