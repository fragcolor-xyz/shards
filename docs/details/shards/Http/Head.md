This shard can accept a string table as input to append query parameters to the specified URL.
For example,
```shards
{
  "param1": "value1"
  "param2": "value2"
  "param3": "value3"
} = queryParams

queryParams
Http.Get("https://api.example.com/endpoint")
```
The Http.Get shard will append the query parameters to the URL and the resulting constructed URL will be: `https://api.example.com/endpoint?param1=value1&param2=value2&param3=value3`

This shard is similar to the Http.Get shard, but it only returns the headers of the response and not the body.
