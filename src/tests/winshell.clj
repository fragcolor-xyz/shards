(def! n (Node))
(schedule
 n
 (Chain
  "n"
  (Msg "command")
  ;; "cmd /C dir"
  ;; (Process.Exec) (Log)
  "" (Process.Run "cmd.exe" ["/C" "dir"])
  (Log)))

(def! dec (fn* [a] (- a 1)))
(def! Loop (fn* [count] (do
  (if (tick n) nil (throw "tick failed"))
  (sleep 0.5)
  (if (> count 0) (Loop (dec count)) nil)
)))

(Loop 5)