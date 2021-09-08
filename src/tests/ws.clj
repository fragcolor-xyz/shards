; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Node))

(def test
  (Chain
   "ws-test"
   :Looped
   (WebSocket.Client "ws1" "echo.websocket.org")
   "Hello websockets!"
   (WebSocket.WriteString .ws1)
   (WebSocket.ReadString .ws1)
   (Log "output")
   (Assert.Is "Hello websockets!" true)
   (Pause 1.0)))

(schedule Root test)
(run Root 0.5 20)

(def test nil)
(def Root nil)