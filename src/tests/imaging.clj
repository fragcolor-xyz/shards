(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  (LoadImage "../../assets/simple1.png")
  (Ref .img)
  (Repeat (-->
           (Get .img)
           (Convolve 2)
           (Log)
           )
          10)
  (Log)))

(run Root 0.1)
