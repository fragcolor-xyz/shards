(def Root (Node))

(def remote "Network.Server.RemoteAddr")

(def network-test-server
  (Chain "server" :Looped
         (Network.Server
          (-->
           (Ref "pkt")) ; the input of this flow will be the received packet
           (Get remote)  ; Context variable Network.Server.RemoteAddr will be the current remote
           (Cond
            [(-->
              (Is "127.0.0.1"))
             (-->
              (Msg "Hey localhost!"))
             (--> true)
             (-->
              (Msg "Hey remote host!"))
             ])
           (Get "pkt")
           (Log)
           ))))

(def network-test-client
  (Chain "client" ; no loop, just test few sends
         "Hey server"
         (Network.Send "127.0.0.1")
         2019
         (Network.Send "127.0.0.1")
         99.9
         (Network.Send "127.0.0.1")
         (Float4 3 2 1 0)
         (Network.Send "127.0.0.1")
         (Const [1 2 3 4 5])
         (Network.Send "127.0.0.1")
           
         
