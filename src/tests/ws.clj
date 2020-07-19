(def Root (Node))

(def test
  (Chain
   "ws-test"
   (WS.Client "echo.websocket.org")))

(schedule Root test)
(run Root 0.1)