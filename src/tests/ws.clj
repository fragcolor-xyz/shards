; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def test
  (Wire
   "ws-test"
   (WebSocket.Client "ws1" "echo.websocket.events" :Target "/ws" :Secure false :Port 80)
   (Pause 2.0)
   "Hello websockets!"
   (WebSocket.WriteString .ws1)
   (Log "Sent")
   (WebSocket.ReadString .ws1)
   (Log "Received")
   (Assert.Is "echo.websocket.events sponsored by Lob.com" true)
   (Pause 1.0)))

(schedule Root test)
(run Root 0.5)

(def test nil)
(def Root nil)