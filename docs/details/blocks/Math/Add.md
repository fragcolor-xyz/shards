Addition is a binary operation, i.e. it needs two arguments to give a result.

The `Math.Add` block takes in the **Input** and the parameter `:Operand` to produce the **Output**.

Both **Input** and `:Operand` can be an integer, a float, or a sequence of such entities (but both value types should match for a given operation). The **Output** is generally of the same type as the **Input** provided to the block. 

## Binary operations on sequences ##
*Taking `Math.Add` operator as an example.*

If sequences are passed as arguments, the operator takes pairs of correspondingly placed elements from these sequences and computes the result for each pair. This gives rise to different scenarios.

1. **Input** and `:Operand` sequence sizes are equal

    Since each element in **Input** sequence has a corresponding element in `:Operand` sequence, the **Output** sequence also has the same number of resultant elements and hence the same size as the argument sequences.
    
    | Entity     | Sequence            | Seq. Size |
    | -----------| --------------------| ----------|
    | **Input**   | [a b c]             | 3         |
    | `:Operand` | [1 2 3]             | 3         |
    | **Output**  | [(a+1) (b+2) (c+3)] | 3         |

2. **Input** sequence size < `:Operand` sequence size
    
    Here a few `:Operand` elements (`:Operand` sequence size - **Input** sequence size) will have no **Input** elements to pair off with. These `:Operand` elements are ignored in the final **Output**. Hence the **Output** sequence size here will the same as the **Input** sequence size.

    | Entity     | Sequence                              | Seq. Size |
    | -----------| --------------------------------------| ----------|
    | **Input**   | [a b]                                 | 2         |
    | `:Operand` | [1 2 3]                               | 3         |
    | **Output**  | [(a+1) (b+2) (_+3)] => [(a+1) (b+2)]  | 2         |

3. **Input** sequence size > `:Operand` sequence size

    Once all the `:Operand` elements have been paired off and computed with the corresponding **Input** elements, the remaining **Input** elements (**Input** sequence size - `:Operand` sequence size) will continue looping over the `:Operand` sequence till all of the **Input** sequence elements have been used. As a result the **Output** sequence will again be the same size as the **Input** sequence.

    | Entity     | Sequence                              | Seq. Size |
    | -----------| --------------------------------------| ----------|
    | **Input**   | [a b c d e]                           | 5         |
    | `:Operand` | [1 2]                                 | 2         |
    | **Output**  | [(a+1) (b+2) (c+1) (d+2) (e+1)]       | 5         |

!!! note
    Such sequence operations are useful in transforming and translating 2D/3D grid values (a frequent requirement in graphics rendering). This is done by passing the transform inputs as an **Input** sequence (to be applied to every row/line for a 2D grid or to every 2D-matrix/plane for a 3D grid) of the 2D matrix and the `:Operand` sequence as the set of 2D/3D coordinates (represented linearly) that is to be transformed.
