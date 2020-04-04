(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  (LoadImage "../../assets/simple1.png")
  (WritePNG "testbase.png")
  (Ref .img)
  (Repeat (-->
           (Get .img)
           (Convolve 20)
           (Log)
           (WritePNG "test.png")
           )
          30)
  (Log)))

(run Root 0.1)
