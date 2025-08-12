 For the first chunk:

- It sets up the HTTP response with status code, content type, and any custom headers.
- It sets the response to chunked encoding.
- It writes the response headers asynchronously.

For each chunk (including the first and subsequent ones):
- It takes the input data (which can be a string or bytes).
- It formats the chunk according to the HTTP chunked encoding format (size in hexadecimal followed by CRLF, then the data, then another CRLF).
- It writes this formatted chunk asynchronously to the socket.

If an empty chunk is sent, it's treated as the end of the response, and the shard resets to be ready for a new chunked response.

The shard uses asynchronous I/O operations and suspends execution while waiting for each write operation to complete, allowing other tasks to run in the meantime.