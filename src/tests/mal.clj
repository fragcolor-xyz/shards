(def chain (Chain "test" true))
(def block1 (Do chain))
(prn "Expect test chain deleted under")
(def block1 nil)
(def chain nil)

(def block2 (Do (Chain "inline-chain" true)))
(prn "Expect inline-chain chain deleted under")
(def block2 nil)

(Chain "lost-chain" true)
(prn "Expect lost-chain chain deleted above")
(do (Chain "do-chain" true))
(prn "Expect do-chain chain deleted above")
