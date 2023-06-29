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
   (String.DecodeURI) (Log)
   (Assert.Is "Hello world, this is an escaping test ////" true)

                                        ; params
   1 (ToString)
   (Set .params "postId")
                                        ; GET
   .params
   (Http.Get "https://raw.githubusercontent.com/fragcolor-xyz/vscode-shards-syntax/main/package.json")
   (FromJson)
   (ExpectTable)
   (Take "author")
   (ExpectString)
   (Assert.Is "Fragcolor and contributors" true)
   (Log)

   (Maybe
    (->
     nil
     (Http.Get "https://httpstat.us/200?sleep=5000" :Timeout 1)
     (Log)))
                                        ; POST

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
(if (run Root 0.1 50) nil (throw "Root tick failed"))

; test reusing sockets/streams
(schedule Root test)
(if (run Root 0.1 50) nil (throw "Root tick failed"))

(schedule Root
          (Wire
           "Download"
           nil
           (Http.Get
            :URL "https://ipfs.io/ipfs/QmSsba3SLnAEVGFaEcnpUeRuAb2vrJE2wpLpmRonf6aRrm"
            :Bytes true
            :Timeout 60)
           = .avocado
           "avocado.glb" (FS.Write .avocado :Overwrite true)))
(if (run Root 0.1) nil (throw "Root tick failed"))

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