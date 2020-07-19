(def Root (Node))

(def test
  (Chain
   "ws-test"
   (WebSocket.Client "ws1" "echo.websocket.org")))

(schedule Root test)
(run Root 0.1)

(def test nil)
(def Root nil)