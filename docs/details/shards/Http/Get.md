This shard can accept a string table as input to append query parameters to the specified URL.
For example,
```shards
{
  "param1": "value1"
  "param2": "value2"
  "param3": "value3"
} = query-params

query-params
Http.Get("https://api.example.com/endpoint")
```
The Http.Get shard will append the query parameters to the URL and the resulting constructed URL will be: `https://api.example.com/endpoint?param1=value1&param2=value2&param3=value3`

The Http.Get shard can handle the different responses that could be returned by the server.
- HTML content: A web page's structure and content.
- Plain text: Simple text data.
- JSON (JavaScript Object Notation): Structured data commonly used in APIs.
- XML (eXtensible Markup Language): Another format for structured data.
- Binary data: Such as images, audio files, or documents.
- Status codes: Indicating success (200 OK), redirection (3xx), client errors (4xx), or server errors (5xx).
- Headers: Metadata about the response, like content type, caching instructions, or cookies.
- Empty response: In some cases, with just a status code.