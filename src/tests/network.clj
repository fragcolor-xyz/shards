(def Root (Node))

(def remote "Network.RemoteEndpoint")

(def network-test-server
  (Chain "server" :Looped
         (Network.Server
          "127.0.0.1" 9191
          (--> ; this acts like a callback                                        
           (Ref "pkt") ; the input of this flow will be the received packet
           ;; (Get remote)  ; Context variable Network.RemoteEndpoint will be the current remote
           ;; (Cond
           ;;  [(-->
           ;;    (Is "127.0.0.1"))
           ;;   (-->
           ;;    (Msg "Hey localhost!"))
           ;;   (--> true)
           ;;   (-->
           ;;    (Msg "Hey remote host!"))
           ;;   ])
           (Get "pkt")
           (Log)
                                        ; send something to the client
                                        ; will use automatically the context vars
           ; "Ok"
           ; (Network.Send)
           ))))

(def client-init
  (Chain "init"
         "Hey server"
         (Network.Send)
         2019
         (Network.Send)
         99.9
         (Network.Send)
         (Float4 3 2 1 0)
         (Network.Send)
         (Const [1 2 3 4 5])
         (Network.Send)
         ))

(def network-test-client
  (Chain "client" :Looped
         (Network.Client
          "127.0.0.1" 9191
          (--> ; acting as callback when there is a new pkt
           (Log)
           ))
                                        ; test some sending
                                        ; will send to the client in context
                                        ; which is the above
                                        ; Network.RemoteEndpoint
                                        ; are the injected variables
         (DispatchOnce client-init)
         ))

(schedule Root network-test-server)
(schedule Root network-test-client)

(run Root 0.02)
          
