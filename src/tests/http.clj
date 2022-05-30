; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(def test
  (Wire
   "test"
   :Looped

   "Hello world, this is an escaping test ////"
   (String.EncodeURI) (Log)
   (Assert.Is "Hello%20world%2C%20this%20is%20an%20escaping%20test%20%2F%2F%2F%2F" true)

                                        ; params
   1 (ToString)
   (Set .params "postId")
                                        ; GET
   .params
   (Http.Get "https://jsonplaceholder.typicode.com/comments")
   (FromJson)
   (ExpectSeq)
   (Take 0)
   (ExpectTable)
   (Take "name")
   (Assert.Is "id labore ex et quam laborum" true)
   (Log)

   (Maybe
    (->
     nil
     (Http.Get "https://httpstat.us/200?sleep=5000" :Timeout 1)
     (Log)))
                                        ; POST
   .params
   (Http.Post "https://jsonplaceholder.typicode.com/posts") &> .json
   (FromJson)
   (ExpectTable)
   (Take "id")
   (Assert.Is 101 true)
   (Log)

   .json
   (Http.Post "https://postman-echo.com/post")
   (Log)

   .json
   (Http.Post "https://postman-echo.com/post" :FullResponse true)
   (Log)
   (Take "headers")
   (Take "content-type")
   (Log)
   (Assert.Is "application/json; charset=utf-8" true)))

(schedule Root test)
(run Root 0.1 50)

; test reusing sockets/streams
(schedule Root test)
(run Root 0.1 50)

(schedule Root
          (Wire
           "Download"
           nil (Http.Get "https://ipfs.io/ipfs/QmSsba3SLnAEVGFaEcnpUeRuAb2vrJE2wpLpmRonf6aRrm" :Bytes true) = .avocado
           "avocado.glb" (FS.Write .avocado :Overwrite true)))
(run Root 0.1)

;; (def server-handler
;;   (Wire
;;    "server-handler"
;;    :Looped
;;    (Msg "handled")
;;    (Http.Read)
;;    (Log)
;;    1 >> .r
;;    2 >> .r
;;    3 >> .r
;;    .r (ToJson) (Http.Response)))

;; (def test-server
;;   (Wire
;;    "test"
;;    :Looped
;;    (Http.Server :Handler server-handler)))

;; (schedule Root test-server)
;; (run Root 0.1)