; we compress a cbl script that will populate the strings at runtime

(def strings (export-strings))

(defn process [s] [(nth s 0) (nth s 1)])

(def payload (apply vector (map process strings)))

(prn payload)

(def Root (Node))

(def compress
  (Chain
   "compress"
   payload
   (ToBytes) >= .source
   (Count .source) (Log "uncompressed size")
   .source (Brotli.Compress) >= .res
   "constexpr std::array<uint8_t, " >= .code
   (Count .res) (Log "compressed size") (ToString) (AppendTo .code)
   "> __chainblocks_compressed_strings = {\n" (AppendTo .code)
   .res (BytesToInts)
   (ForEach ~[>= .b
              "  " (AppendTo .code)
              .b (ToHex) (AppendTo .code)
              ",\n" (AppendTo .code)])
   "};\n" (AppendTo .code)
   "cbccstrings.hpp"
   (FS.Write .code :Overwrite true)))

(prepare compress)
(start compress)
(tick compress)
(stop compress)
