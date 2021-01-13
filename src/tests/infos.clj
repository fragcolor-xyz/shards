; strings are compressed by default, need to unpack if we use info
(decompress-strings)
(prn (map (fn* [name] [name (info name)]) (blocks)))
