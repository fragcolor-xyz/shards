(def Root (Node))

(def test
  (Chain
   "test"
   :Looped
   (Http.Get "api.binance.com" "/api/v3/klines?symbol=BNBBTC&interval=15m")
   (Log)
   ))

(schedule Root test)
(run Root 0.1 10)
