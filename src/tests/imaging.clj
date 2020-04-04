(def Root (Node))

(schedule
 Root
 (Chain
  "test"
  (LoadImage "../../assets/simple1.png")
  (StripAlpha)
  (WritePNG "testbase.png")
  (Ref .img)
  (Repeat (-->
           (Get .img)
           (Convolve 50)
           (WritePNG "test.png")
           (Log)
           (ImageToFloats)
           (Ref .s)
           (Count .s)
           (Log)
           (Get .s)
           (FloatsToImage 99 99 3)
           (WritePNG "test2.png")
           )
          30)
  (Log)))

(run Root 0.1)
