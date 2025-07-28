A HTTP POST request is a method of sending data to a server to create a resource.

This request is not idempotent. Multiple identical requests might create multiple resources.

This shard can accept a string table, string or bytes as input and will modify the body of the POST request accordingly.
- **none** : no body is attached to the request
- **string** : The string is attached to the body as is and treated as a JSON string.
- **string table** : Both key and values need to be strings. The shard constructs a URL encoded string using the key and value pairs from the table and attaches it to the body of the request.
- **bytes** : The bytes are attached to the body as is.

If the output of the shard is a table, it will have the following format:
```shards
{
  status: @type(Type::Int)
  headers: {header: @type(Type::String)}
  body: @type(Type::String) ;; or @type(Type::Bytes)
}
```
- **status**: An integer representing the HTTP status code.
- **headers**: A table where both keys and values are strings, representing the response headers.
- **body**: Either a string or bytes (depending on the asBytes parameter), containing the response body.

