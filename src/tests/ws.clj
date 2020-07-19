(def Root (Node))

(def test
  (Chain
   "ws-test"
   (WebSocket.Client "echo.websocket.org")))

(schedule Root test)
(run Root 0.1)