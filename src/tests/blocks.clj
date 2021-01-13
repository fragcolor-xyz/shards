;; (prn (blocks))
(prn (map (fn* [name] [name (info name)]) (blocks)))
(prn (export-strings))
