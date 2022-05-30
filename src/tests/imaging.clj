; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(schedule
 Root
 (Wire
  "test"
  (LoadImage "../../assets/simple1.PNG") >= .baseImg
  (StripAlpha)
  (WritePNG "testbase.png")
  (Ref .img)
  (Repeat (->
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
           (FillAlpha)
           (ResizeImage 200 200)
           (WritePNG "test2Resized.png"))
          30)
  (Log)
  .baseImg
  (ResizeImage 200 200)
  (WritePNG "testResized.png")))

(run Root 0.1)
