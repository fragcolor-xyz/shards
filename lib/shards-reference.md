# Shards Reference

# Replace
This shard replaces all occurrences of the pattern(specified in the Patterns parameter) found in the input sequence or string, with replacements (specified in the Replacements parameter).

I/O Types [Any]/String → [Any]/String

Parameters
Patterns None/[String]/Var([String])/Var([Any])/[Any]
Replacements None/Any/Var(Any)/[Any]/Var([Any])

# Regex.Replace
This shard modifies the input string by replacing all occurrences of the regex pattern, specified in the Regex parameter, with the replacement string specified in the Replacement parameter.

I/O Types String → String

Parameters
Regex String
Replacement String/Var(String)

# Math.Sinh
This shard calculates the hyperbolic sine of the given input, where the input is the real number. The hyperbolic sine is a hyperbolic function that is analogous to the circular sine function, but it uses exponential functions instead of angles.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Floor
This shard rounds down the input to the nearest integer.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Pass
This shard is a "no operation" shard. It simply passes through the input without modifying it.

I/O Types Any → Any

# ExpectIntSeq
Checks if the input value is a sequence of Ints. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int]

Parameters
Unsafe Bool

# String.Format
This shard concatenates all the elements of a sequence into a string

I/O Types [Any] → String

# Detach
Schedules and executes the specified Wire asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Spawn, only one unique copy of the specified Wire can be scheduled using Detach. Future calls of Detach that schedules the same Wire will be ignored unless the specified Wire is Stopped or ends naturally.

I/O Types Any → Any

Parameters
Wire Wire/String/None
Restart Bool

# ImageToFloats
Convert an image into a sequence of floats. Each pixel in the image is converted to a float value between 0 and 1 and stored in the sequence.

I/O Types Image → [Float]

# Input
The input value of the wire.

I/O Types None → Any

# Math.RShift
This shard shifts the bits of the input value to the right by the number of positions specified in the Operand parameter. The shard then outputs a value, whose binary representation is the resulting shifted binary.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

Parameters
Operand Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

# Asin
This shard calculates the inverse sine of the given input, where the input is the sine value. The output is the angle in radians whose sine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Pow
This shard raises the input to the power of the exponent specified in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Memoize
Computes a value

I/O Types Any → Any

Parameters
Evaluate Shard/[Shard]

# Dec
Decreases the input by 1.

I/O Types Any → Any

Parameters
Value Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Time.EpochLocalMs
This shard outputs the amount of time that has elapsed from the Unix epoch to the current local system time in milliseconds.

I/O Types None → Int

# Time.Epoch
This shard outputs the the amount of time that has elapsed from the Unix epoch to the current system time in seconds.

I/O Types None → Int

# Time.Delta
Outputs the time between the last call of this shard and the current call in seconds, capped to a limit

I/O Types None → Float

# Time.NowMs
This shard outputs the amount of time that has elapsed since the shards application or script was launched in milliseconds.

I/O Types None → Float

# FlushLog
This shard flushes the log buffer to the console. This ensures that any pending log messages are immediately written to the console.

I/O Types Any → Any

# Msg
Displays the passed message string to the user via standard output. The input variable is ignored, and only the static message is displayed.

I/O Types Any → Any

Parameters
Message String/Var(String)
Raw Bool
Level LogLevel/Var(LogLevel)
Name String/Var(String)

# WhenDone
Schedules the specified Wire and runs it asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Detach, a copy of the specified Wire is scheduled every time the shard is called.

I/O Types Any → Any

Parameters
Wire None/Wire/[Shard]

# Suspend
Pauses a specified Wire's execution. If no Wire is specified, pauses the current wire.

I/O Types Any → Any

Parameters
Wire Wire/String/None/Var(Wire)

# IsRunning
Checks if a Wire is running and outputs true if it is, false if otherwise. (Note that a looped Wire will always be running and thus will always return true)

I/O Types None → Bool

Parameters
Wire Wire/String/None/Var(Wire)

# DoMany
This shard takes a sequence of values as input, schedules multiple copies of a specified Wire and executes them sequentially. Each value from the sequence is provided as input to its corresponding copy of the specified Wire. The shard then outputs a sequence of values containing the output of all copies of the specified Wire.

I/O Types [Any] → [Any]

Parameters
Wire None/Wire/[Shard]
ComposeSync Bool

# Math.MatMul
Performs matrix multiplication on either two matrices or a matrix and a vector and outputs either a matrix or a vector accordingly. The two matrixes must be of similar dimensions (2x2, 3x3, or 4x4). And if multiplied with a vector, the vector too must have similar dimensions (2x2 with float2, 3x3 with float3, 4x4 with float4).

I/O Types [Float4](4)/[Float3](3)/[Float2](2) → [Float4](4)/Float4/[Float3](3)/Float3/[Float2](2)/Float2

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Spawn
Schedules the specified Wire and runs it asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Detach, a copy of the specified Wire is scheduled every time the shard is called.

I/O Types Any → Wire

Parameters
Wire None/Wire/[Shard]

# ToHex
Converts an integer, bytes, or string value into its hexadecimal string representation.

I/O Types Int/Int16/Bytes/String → String

# TryMany
This shard takes a sequence of values as input, schedules multiple copies of a specified Wire and executes them asynchronously. Each value from the sequence is provided as input to its corresponding copy of the scheduled Wire. The shard will then wait for all the scheduled Wires to end, and then, depending on the Policy specified, the shard will either return the output of the first successful Wire, return a sequence with all the output from all the copies of the specified Wire or stop execution of the current Wire if all the copies failed.

I/O Types [Any] → [Any]

Parameters
Wire None/Wire/[Shard]
Policy WaitUntil
Threads Int

# Sin
This shard calculates the sine of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Step
The first time Step is called, the specified wire is scheduled. On subsequent calls, the specified Wire's state is progressed before the current Wire continues its execution. This means that a pause in execution of the child Wire will not pause the parent Wire.

I/O Types Any → Any

Parameters
Wire Wire/String/None

# TypeOf
Evaluates the output type of the given expression specified by the 'OutputOf' parameter and outputs that type. No input is required for this shard.

I/O Types None → Type

Parameters
OutputOf Shard/[Shard]/None

# ExpectImage
Checks the input value if it is an Image. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Image

# Stop
Either stops the execution of a specified Wire or the current Wire.

I/O Types Any → Any

Parameters
Wire Wire/String/None/Var(Wire)
Passthrough Bool

# Wait
Waits for the specified Wire to complete before resuming execution of the current Wire.

I/O Types None → Any

Parameters
Wire Wire/String/None/Var(Wire)
Passthrough Bool
Timeout Float/Var(Float)/None

# String.FromCodePoints
Converts a sequence of integer codepoints into a string.

I/O Types [Int] → String

# String.CodePoints
Converts a string into a sequence of integer codepoints.

I/O Types String → [Int]

# String.Starts
This shard checks if the input string starts with the string specified in the With parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

I/O Types String → Bool

Parameters
With String/Var(String)

# String.Split
This shard splits the input string into a sequence of its constituent strings, using the string specified in the Separator parameter to segment the input. If the KeepSeparator parameter is true, the separator will be included in the output.

I/O Types String → [String]

Parameters
Separator String/Var(String)
KeepSeparator Bool

# String.RFind
Finds the last occurence of the string specified in the String parameter in the input string and outputs the index of the first occurence.

I/O Types String → Int

Parameters
ToFind String/Var(String)

# String.Contains
This shard checks if the input string contains the string specified in the String parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

I/O Types String → Bool

Parameters
String String/Var(String)

# String.Trim
This shard removes all leading and trailing whitespace characters from the input string and outputs the trimmed string.

I/O Types String → String

# String.ToLower
This shard converts all characters in the input string to lowercase.

I/O Types String → String

# Regex.Match
This shard matches the entire input string against the regex pattern specified in the Regex parameter and outputs a sequence of strings, containing the fully matched string and any capture groups. It will return an empty sequence if there are no matches.

I/O Types String → [String]

Parameters
Regex String

# Regex.Search
This shard searches the input string for the regex pattern specified in the Regex parameter and outputs a sequence of strings, containing every occurrence of the pattern. An empty sequence is returned if there are no matches

I/O Types String → [String]

Parameters
Regex String

# Zip
Zip will take any number of sequences and return a sequence of sequences, where each sequence is a tuple of the values from the input sequences at the same index.

I/O Types None → [{Any}]/[[Any]]

Parameters
Sequences [[Any] Var([Any])]
Keys None/[String]

# IndexOf
This shard will search the input sequence for the index of an item or a pattern of items (specified in the Item parameter) and return its index(or a sequence of indices).

I/O Types [Any] → [Int]/Int

Parameters
Item Any
All Bool
Predicate Shard/[Shard]

# Flatten
This shard will take a sequence with nested values (eg. a sequence of sequences or a sequence of tables) and create a single sequence with all of values, nested values and keys as elements.

I/O Types Any → Any

# Math.Percentile
This shard calculates the percentile of the input value within the specified sequence.

I/O Types [Float] → Float

Parameters
Percentile Float/Var(Float)

# Lerp
Linearly interpolate between the start value specified in the `First` parameter and the end value specified in the `Second` parameter based on the factor provided as input.

I/O Types Float → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
First Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)
Second Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)

# Percentile
This shard calculates the percentile of the input value within the specified sequence.

I/O Types [Float] → Float

Parameters
Percentile Float/Var(Float)

# Math.Pow
This shard raises the input to the power of the exponent specified in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# WireComposer
Attempts to compose the specified wire and outputs "OK" if successful, or an error message if the composition fails.

I/O Types None → String

Parameters
Wire Wire/Var(Wire)

# Math.Dec
Decreases the input by 1.

I/O Types Any → Any

Parameters
Value Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Inc
Increases the input by 1.

I/O Types Any → Any

Parameters
Value Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Math.Round
This shard rounds the input floating-point number to the nearest integer.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Trunc
This shard truncates the input floating-point number towards zero, removing any fractional part without rounding.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Ceil
This shard rounds up the input to the nearest integer.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Floor
This shard rounds down the input to the nearest integer.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Ceil
This shard rounds up the input to the nearest integer.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.LGamma
This shard calculates the log gamma function of the given input. The log gamma function is the natural logarithm of the absolute value of the gamma function.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Asinh
This shard calculates the inverse hyperbolic sine of the given input, where the input is the hyperbolic sine value. The output is the real number whose hyperbolic sine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Log1p
This shard adds 1 to the input and then calculates the natural logarithm of the result.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Tanh
This shard calculates the hyperbolic tangent of the given input, where the input is the real number. The hyperbolic tangent is a hyperbolic function that is analogous to the circular tangent function, but it uses exponential functions instead of angles.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Asin
This shard calculates the inverse sine of the given input, where the input is the sine value. The output is the angle in radians whose sine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Tan
This shard calculates the tangent of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Sin
This shard calculates the sine of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.FastInvSqrt
This shard calculates the inverse square root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.FastSqrt
This shard calculates the square root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Sqrt
This shard calculates the square root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# IntsToBytes
Converts a sequence of integers into a byte array. Each integer in the sequence is serialized into its binary representation and concatenated into the resulting byte array.

I/O Types [Int] → Bytes

# HexToBytes
Converts a hexadecimal string to its original byte array representation.

I/O Types String → Bytes

# Math.Exp2
This shard calculates the exponential function with base 2 for the given input. The exponential function with base 2 is equivalent to raising 2 to the power of the input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Fail
Stops the current flow and cancels the execution with the provided error message. This shard is used to signal an error and halt the execution of the current wire.

I/O Types String → None

# Math.Exp
This shard calculates the exponential function with base e (Euler's number) for the given input. The exponential function is equivalent to raising Euler's number to the power of the input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Abs
This shard outputs the absolute value of the input.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

# Math.Abs
This shard outputs the absolute value of the input.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

# Math.LShift
This shard shifts the bits of the input value to the left by the number of positions specified in the Operand parameter. The shard then outputs a value, whose binary representation is the resulting shifted binary.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

Parameters
Operand Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

# Math.Divide
This shard divides the input value by the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Sub
This shard subtracts the value provided in the Operand parameter from the input value.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Add
This shard adds the input value to the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.QuatRotate
This shard rotates the input 3D vector (represented as a float3) by the quaternion (represented as a float4) specified in the Operand parameter and outputs the resulting rotated 3D vector. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

I/O Types Float3 → Float3

Parameters
Operand Float4/Var(Float4)

# Math.Atanh
This shard calculates the inverse hyperbolic tangent of the given input (atanh(x)), where x, outputs y such that tanh(y) = x.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Time.MovingAverage
This shard computes the average of a floating point number over a specified number of frames.

I/O Types Float → Float

Parameters
Window Int
Clear None/Var(Bool)

# Resume
Resumes another Wire (previously paused using Suspend).

I/O Types Any → Any

Parameters
Wire Wire/String/None/Var(Wire)

# Extend
Extends the mutable sequence parameter with the elements of the input sequence.

I/O Types [Any] → [Any]

Parameters
Target Var([Any])

# Math.Unproject
This shard converts 2D screen coordinates back to 3D world coordinates using the inverse of a view-projection matrix. Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 float elements).It performs the reverse operation of the projection pipeline, including inverse matrix multiplication, and coordinate space transformations using the 4x4 view-projection matrix specified in the Matrix parameter and the screen size in the ScreenSize parameter.

I/O Types Float3 → Float3

Parameters
Matrix [Float4](4)/Var([Float4](4))
ScreenSize Float2/Var(Float2)
DepthRange None/Float2/Var(Float2)
FlipY None/Bool/Var(Var(Bool))

# Math.Project
This shard converts the input 3D world coordinates to 2D screen coordinates using a view-projection matrix. Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 float elements).It performs the full projection pipeline including matrix multiplication, perspective division, and viewport transformation using the 4x4 view-projection matrix specified in the Matrix parameter and the screen size in the ScreenSize parameter.

I/O Types Float3 → Float3

Parameters
Matrix [Float4](4)/Var([Float4](4))
ScreenSize Float2/Var(Float2)
FlipY Bool/Var(Var(Bool))

# Math.MatIdentity
This shard creates a standard 4x4 identity matrix. The standard identity matrix is a square matrix with 1s on the main diagonal and 0s for the other elements. A 4x4 matrix is a sequence with exactly 4 float4 vector and a float4 vector is a vector with 4 float elements.

I/O Types None → [Float4](4)

Parameters
Type Type

# DegreesToRadians
This shard converts the input angle from degrees to radians. The conversion is done using the formula: radians = degrees * (π / 180).

I/O Types Float → Float

# Math.DegreesToRadians
This shard converts the input angle from degrees to radians. The conversion is done using the formula: radians = degrees * (π / 180).

I/O Types Float → Float

# Math.AxisAngleZ
This shard creates a rotation quaternion for rotation around the Z-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

I/O Types Float → Float4

# Math.AxisAngleY
This shard creates a rotation quaternion for rotation around the Y-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

I/O Types Float → Float4

# Tan
This shard calculates the tangent of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Rotation
This shard creates a 4x4 rotation matrix (a sequence of four float4 vectors) from a float4 vector input representing a rotation quaternion. A float4 vector is a vector with 4 float elements.

I/O Types Float4 → [Float4](4)

# Math.And
This shard performs a bitwise AND operation on the input value with the value specified in the Operand parameter and outputs the result. A bitwise AND operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if the bits are 1 and 0 otherwise. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the AND comparison.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool → Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool

Parameters
Operand Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# Math.Scaling
This shard creates a 4x4 scaling matrix (a sequence of four float4 vectors) from a float3 vector input that represents the scaling factors in x, y, and z directions. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

I/O Types Float3 → [Float4](4)

# Math.Inverse
This shard takes a 4x4 matrices as input and computes its inverse. A 4x4 matrix is a sequence with exactly 4 float4 vectors while a float4 vector is a vector with 4 float elements.

I/O Types [Float4](4) → [Float4](4)

# Math.Transpose
Performs matrix transposition on the input matrix. Transposition flips the matrix over its main diagonal, switching its rows and columns. This shard supports 2x2, 3x3, and 4x4 as input matrices. A 4x4 matrix is a sequence with exactly 4 float4 vectors, a 3x3 matrix is a sequence with exactly 3 float3 vectors, and a 2x2 matrix is a sequence with exactly 2 float2 vectors.

I/O Types [Float4](4)/[Float3](3)/[Float2](2) → [Float4](4)/[Float3](3)/[Float2](2)

# Math.Length
Computes the magnitude of a float vector of any dimension and outputs the result as a float.

I/O Types Float2/[Float2]/Float3/[Float3]/Float4/[Float4] → Float

# Math.LengthSquared
Computes the squared magnitude of a float vector of any dimension and outputs the result as a float.

I/O Types Float2/[Float2]/Float3/[Float3]/Float4/[Float4] → Float

# IntRange
Returns a sequence of integers from Start (inclusive) to End (exclusive)

I/O Types None → [Int]

Parameters
Start Int/Var(Int)
End Int/Var(Int)

# Math.Normalize
This shard normalizes a float vector of any dimension or a sequence of floats, scaling it to have a magnitude of 1 while preserving its direction. By default, output values can range from -1.0 to 1.0. If the 'Positive' parameter is set to true, the output will be scaled to the range 0.0 to 1.0. For example, normalizing [4.0 -5.0 6.0 -7.0] will result in [0.3563, -0.4454, 0.5345, -0.6236], which has a length of 1. 

I/O Types [Float]/Float2/[Float2]/Float3/[Float3]/Float4/[Float4] → [Float]/Float2/[Float2]/Float3/[Float3]/Float4/[Float4]

Parameters
Positive Bool

# Math.Dot
Computes the dot product of two float vectors with an equal number of elements, and outputs the resulting float value. The first float vector is passed as input and the second float vector is specified in the Operand parameter.

I/O Types Float2/[Float2]/Float3/[Float3]/Float4/[Float4] → Float2/[Float2]/Float3/[Float3]/Float4/[Float4]

Parameters
Operand Float2/[Float2]/Float3/[Float3]/Float4/[Float4]/Var(Float2)/Var([Float2])/Var(Float3)/Var([Float3])/Var(Float4)/Var([Float4])

# Math.Xor
This shard performs a bitwise XOR operation on the input with the value specified in the Operand parameter and outputs the result. A bitwise XOR operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if the bits are different and 0 if they are the same. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the XOR comparison.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool → Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool

Parameters
Operand Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# If
Evaluates a predicate and executes an action.

I/O Types Any → Any

Parameters
Predicate Shard/[Shard]/None
Then Shard/[Shard]/None
Else Shard/[Shard]/None
Passthrough Bool

# Max
This shard compares the input with the value specified in the `Operand` parameter and outputs the larger value.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# When
Conditional shard that only executes the action if the predicate is true.

I/O Types Any → Any

Parameters
Predicate Shard/[Shard]/None
Action Shard/[Shard]/None
Passthrough Bool

# Maybe
Attempts to activate a shard or a sequence of shards. Upon failure, activate another shard or sequence of shards.

I/O Types Any → Any

Parameters
Shards Shard/[Shard]/None
Else Shard/[Shard]/None
Silent Bool

# FromBase64
Decodes a Base64 encoded string to its original byte representation.

I/O Types String → Bytes

# ToBase64
Encodes the input bytes or string value to its Base64 string representation.

I/O Types Bytes/String → String

# BytesToAudio
Converts a byte array containing float samples back into an audio buffer.

I/O Types Bytes → Audio

Parameters
Channels Int
SampleRate Int

# AudioToBytes
Converts an audio buffer into a byte array.

I/O Types Audio → Bytes

# ImageToBytes
Converts an image into a byte array.

I/O Types Image → Bytes

# BytesToInts
Convert bytes into a sequence of integers. Each byte is interpreted as an integer and stored in the sequence.

I/O Types Bytes → [Int]

# ExpectInt3
Checks the input value if it is a vector with three Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int3

# Math.Cbrt
This shard calculates the cube root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# IsTable
Checks the input value if it is a Table. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# WhenNot
Conditional shard that only executes the action if the predicate is false.

I/O Types Any → Any

Parameters
Predicate Shard/[Shard]/None
Action Shard/[Shard]/None
Passthrough Bool

# IsSeq
Checks the input value if it is of the type specified. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsBool
Checks the input value if it is a Boolean. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsBytes
Checks the input value if it is of type Bytes. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsFloat3
Checks the input value if it is a vector with three Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsFloat2
Checks the input value if it is a vector with two Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsFloat
Checks the input value if it is of type Float. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsInt8
Checks the input value if it is a vector of 8 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsInt4
Checks the input value if it is a vector of 4 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsInt3
Checks the input value if it is a vector of 3 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# IsInt2
Checks the input value if it is a vector of 2 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# Math.Mod
This shard calculates the remainder of the division of the input value by the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# ExpectAudioSeq
Checks if the input value is a sequence of Audio buffers. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Audio]

Parameters
Unsafe Bool

# ExpectWireSeq
Checks if the input value is a sequence of Wires. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Wire]

Parameters
Unsafe Bool

# ExpectColorSeq
Checks if the input value is a sequence of Color vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Color]

Parameters
Unsafe Bool

# ExpectImageSeq
Checks if the input value is a sequence of Images. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Image]

Parameters
Unsafe Bool

# ExpectBytesSeq
Checks if the input value is a sequence of Bytes. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Bytes]

Parameters
Unsafe Bool

# ExpectInt16Seq
Checks if the input value is a sequence of Int16 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int16]

Parameters
Unsafe Bool

# ExpectInt8Seq
Checks if the input value is a sequence of Int8 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int8]

Parameters
Unsafe Bool

# RadiansToDegrees
This shard converts the input angle from radians to degrees. The conversion is done using the formula: degrees = radians * (180 / π).

I/O Types Float → Float

# ExpectInt3Seq
Checks if the input value is a sequence of Int3 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int3]

Parameters
Unsafe Bool

# ExpectInt2Seq
Checks if the input value is a sequence of Int2 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int2]

Parameters
Unsafe Bool

# ExpectFloat4Seq
Checks if the input value is a sequence of Float4 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Float4]

Parameters
Unsafe Bool

# Hash
This shard takes any input type, hashes them using the XXH128 hashing algorithm and outputs their 128-bit hash value as an int2 (a sequence with 2 integers as elements).

I/O Types Any → Int2

# ExpectFloat3Seq
Checks if the input value is a sequence of Float3 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Float3]

Parameters
Unsafe Bool

# WireRunner
Runs the wire variable specified by the input wire variable.

I/O Types Any → Any

Parameters
Wire Wire/Var(Wire)
Mode RunWireMode

# ExpectWire
Checks the input value if it is a Wire. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Wire

# Expand
Schedules (n) number of copies of the specified Wire, where (n) is the number specified in the Size parameter. The parent Wire will wait until all the scheduled copies have ended and will either return a sequence of values outputs of all the copied Wires or the output of the first Wire that succeeds. Once done, it will continue with its own execution.

I/O Types Any → [Any]

Parameters
Size Int
Wire None/Wire/[Shard]
Policy WaitUntil
Threads Int

# ExpectColor
Checks the input value if it is vector of four color channels (RGBA). The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Color

# ExpectString
Checks the input value if it is a String. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → String

# ExpectFloat4
Checks the input value if it is a vector with float Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Float4

# ExpectFloat2
Checks the input value if it is a vector with two Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Float2

# ExpectFloat
Checks the input value if it is of type Float. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, it will fail.

I/O Types Any → Float

# ExpectInt16
Checks the input value if it is a vector with sixteen Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int16

# Math.Acos
This shard calculates the inverse cosine of the given input, where the input is the cosine value. The output is the angle in radians whose cosine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# ExpectInt4
Checks the input value if it is a vector with four Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int4

# ExpectInt2
Checks the input value if it is a vector with two Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int2

# ExpectInt
Checks the input value if it is of type Int. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int

# BitSwap64
This shard takes a 64-bit integer, reverses their order of its bytes, and outputs the result as an integer. This is useful for converting between different endianness formats.

I/O Types Int → Int

# BitSwap32
This shard takes a 32-bit integer, reverses their order of its bytes, and outputs the result as an integer. This is useful for converting between different endianness formats.

I/O Types Int → Int

# Math.Compose
Creates a 4x4 transformation matrix (sequence of four float4 vectors) from a table containing the appropriate Translation, Rotation and Scale values. values. The translation value should be a float3 vector representing positions on the x y z axis. The rotation value should be a float4 vector representing the quaternion rotation. Lastly, the scale should be a float3 vector representing the size on the x y and z axis. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements.

I/O Types {translation: Float3 rotation: Float4 scale: Float3} → [Float4](4)

# ExpectNone
Checks the input value if it is none. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → None

# ToFloat3
Converts various input types to a vector of three Float elements. If a single value or a collection with less than 3 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# Last
Returns the last element from a sorted table or sequence. For tables, returns a [key, value] pair. Returns None if empty. Note: This operation is fast but unsafe unless the output is cloned (using Set instead of Ref) when combined with await or suspended wire flow.

I/O Types [Any]/{Any} → Any

# ToInt16
Converts various input types to a vector of sixteen Int elements. If a single value or a collection with less than 16 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# Mul
This shard multiplies the input value by the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# ToInt8
Converts various input types to a vector of eight Int elements. If a single value or a collection with less than 8 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# Await
Executes a shard or a sequence of shards asynchronously and awaits their completion.

I/O Types Any → Any

Parameters
Shards Shard/[Shard]/None

# ToInt3
Converts various input types to a vector of three Int elements. If a single value or a collection with less than 3 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# ToInt2
Converts various input types to a vector of two Int elements. If a single value or a collection with only one element is provided, the second element in the resulting vector will be set to 0.

I/O Types Any → Any

# Iterate
Searches through a sorted table input for a range of matching elements. Returns all values from the table that have keys between the From and To keys.

I/O Types {Any} → {Any}

Parameters
From Any
To Any/None
Action Shard/[Shard]/None

# Shards.EnumTypeHelp
Returns a table of help information for the enum type specified by the input id.

I/O Types Int → {Any}

# Shards.ObjectTypes
Returns a sequence of all object types in the system.

I/O Types None → [Int]

# String.Find
Finds the next occurence of the string specified in the String parameter in the input string and outputs the index of the first occurence.

I/O Types String → Int

Parameters
ToFind String/Var(String)

# ParseFloat
Converts the string representation of a number to its floating-point number equivalent.

I/O Types String → Float

# NaNTo0
Replaces NaN (Not a Number) values in the input with 0. This shard can handle both single float values and sequences of float values.

I/O Types Float/[Float] → Float/[Float]

# Browse
This shard will open the URL string input in the current system's default web browser.

I/O Types String → String

# Math.Inc
Increases the input by 1.

I/O Types Any → Any

Parameters
Value Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# ToString
Converts any input value to its string representation.

I/O Types Any → String

# Lowest
Takes a sequence and outputs the element with the lowest value.

I/O Types [Any] → Any

# Insert
Prepends the input to the context variable passed to `Collection`.

I/O Types Any → Any

Parameters
Index Int/Var(Int)
Collection Var([Any])/Var(String)/Var(Bytes)

# Once
Executes the shard or sequence of shards with the desired frequency in a wire flow execution.

I/O Types Any → Any

Parameters
Action Shard/[Shard]
Every Float/Var(Float)

# Fold
Folds a sequence into a single value by applying an operation (specified in the Apply parameter) to each item of the sequence. The operation can transform the type. Note that this shard is able to use the $0 internal variable for the accumulated value, $1 for the current item, and $i for the current index.

I/O Types [Any] → Any

Parameters
Apply Shard/[Shard]
Initial Any/Var(Any)

# Map
Processes each element of a sequence or key-value pair of a table using the shards specified in the `Apply` parameter and outputs the modified sequence or table. Note that this shard is able to use the $0 and $1 internal variables, as well as $i for the current index.

I/O Types [Any]/{Any} → [Any]

Parameters
Apply Shard/[Shard]

# Math.Not
This shard performs a bitwise NOT operation on the input. It flips all the bits of the input number.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/[Any] → Int/Int2/Int3/Int4/Int8/Int16/[Any]

# FloatsToImage
Converts a sequence of floats into an image. The image dimensions (width and height) and the number of channels are specified by the appropriate parameters.

I/O Types [Float] → Image

Parameters
Width Int
Height Int
Channels Int

# ExpectFloat3
Checks the input value if it is a vector with three Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Float3

# ExpectLike
Checks if the input value matches the type of the value provided in the TypeOf parameter or the output type of the given expression in the OutputOf parameter. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution. Note that it can only compare with either one of the parameters, not both; an error will be thrown if both are provided. The 'Unsafe' parameter can be set to skip deep type hashing and comparison to improve performance.

I/O Types Any → Any

Parameters
TypeOf Any
OutputOf Shard/[Shard]/None
Unsafe Bool

# IsTrue
Gets whether the input is `true`.

I/O Types Bool → Bool

# Log
Logs the output of a shard or the value of a variable to the console along with an optional prefix string. The logging level can be specified to control the verbosity of the log output.

I/O Types Any → Any

Parameters
Prefix String
Level LogLevel/Var(LogLevel)
Name String/Var(String)

# Shards.ObjectTypeHelp
Returns a table of help information for the object type specified by the input id.

I/O Types Int → {Any}

# FromBytes
This shard takes a serialized binary representation of a value and convert it back to its original type.

I/O Types Bytes → Any

# ToColor
Converts various input types to a vector of four color channels (RGBA). If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# IsAudio
Checks the input value if it is an Audio file. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# ToInt4
Converts various input types to a vector of four Int elements. If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# Pause
Pauses the wire for a given amount of time.

I/O Types Any → Any

Parameters
Time None/Float/Int/Var(Float)/Var(Int)

# Merge
Combine two tables into one, with the input table taking priority over the operand table, which will be written and returned as output. This shard is useful in scenarios where you need to merge data from different sources while keeping the priority of certain values.

I/O Types {Any} → {Any}

Parameters
Target Var({Any})

# IsNone
Gets whether the type of the input is `None`.

I/O Types Any → Bool

# Return
Stops the current flow and outputs the provided input. This shard is used to exit the execution of the current wire early within loops or conditional flows, returning the specified input.

I/O Types Any → None

# Math.Subtract
This shard subtracts the value provided in the Operand parameter from the input value.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# LogType
Logs the type of the value to the console along with an optional prefix string. The logging level can be specified to control the verbosity of the log output.

I/O Types Any → Any

Parameters
Prefix String
Level LogLevel/Var(LogLevel)
Name String/Var(String)

# ForEach
Processes every element or key-value pair of a sequence/table with the shards specified in the `Apply` parameter. Note that this shard is able to use the $0 and $1 internal variables, as well as $i for the current index.

I/O Types [Any]/{Any} → [Any]/{Any}

Parameters
Apply Shard/[Shard]

# Math.RadiansToDegrees
This shard converts the input angle from radians to degrees. The conversion is done using the formula: degrees = radians * (180 / π).

I/O Types Float → Float

# ExpectInt4Seq
Checks if the input value is a sequence of Int4 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Int4]

Parameters
Unsafe Bool

# Shards.EnumTypes
Returns a sequence of all enum types in the system.

I/O Types None → [Int]

# Const
Declares an un-named constant value (of any data type).

I/O Types None → Any

Parameters
Value Any

# Cos
This shard calculates the cosine of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Decompose
This shard converts a 4x4 transformation matrix (a sequence of four float 4 vectors) into a table containing its constituent Translation, Rotation, and Scale components. The table has a Translation key with a float3 vector value representing positions on the x, y, z axes, a Rotation key with a float4 vector value representing the quaternion rotation, and a Scale key with a float3 vector value, representing the size on the x, y, z axes. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements. 

I/O Types [Float4](4) → {translation: Float3 rotation: Float4 scale: Float3}

# Math.Cos
This shard calculates the cosine of the given input, where the input is the angle in radians.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Time.EpochLocal
This shard outputs the amount of time that has elapsed from the Unix epoch to the current local system time in seconds.

I/O Types None → Int

# LastError
This shard outputs the last error message that occurred as a string.

I/O Types None → String

# Math.Multiply
This shard multiplies the input value by the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Shards.Help
Returns a table of help information for the shard specified by the input name.

I/O Types String → {Any}

# IsFloat4
Checks the input value if it is a vector with float Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# Isolate
Isolates the inner shards' environment by only allowing certain variables

I/O Types Any → Any

Parameters
Contents Shard/[Shard]
Include None/[String]
Exclude None/[String]

# First
Returns the first element from a sorted table or sequence. For tables, returns a [key, value] pair. Returns None if empty. Note: This operation is fast but unsafe unless the output is cloned (using Set instead of Ref) when combined with await or suspended wire flow.

I/O Types [Any]/{Any} → Any

# ToInt
Converts various input types to type Int.

I/O Types Any → Any

# ExpectTable
Checks the input value if it is a Table. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → {}

# Math.Mean
Calculates the average value of a sequence of floating point numbers.

I/O Types [Float] → Float

Parameters
Kind Mean

# ToFloat
Converts various input types to type Float.

I/O Types Any → Any

# ToFloat4
Converts various input types to a vector of Four Float elements. If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

I/O Types Any → Any

# ForRange
Executes a series of shards while an iteration value is within a specified range.

I/O Types Any → Any

Parameters
From Int/Var(Int)
To Int/Var(Int)
Action Shard/[Shard]/None

# Assoc
Updates a sequence (array) or a table (associative array/ dictionary) on the basis of an input sequence.

I/O Types [Any] → [Any]

Parameters
Name String/Var(Any)
Key Any
Global Bool

# Math.Add
This shard adds the input value to the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# PrependTo
Prepends the input to the context variable passed to `Collection`.

I/O Types Any → Any

Parameters
Collection Var([Any])/Var(String)/Var(Bytes)

# TraitId
Retrieves the hash id of the given trait

I/O Types None → Int2

Parameters
Trait Trait

# ExpectFloat2Seq
Checks if the input value is a sequence of Float2 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Float2]

Parameters
Unsafe Bool

# IsFalse
Gets whether the input is `false`.

I/O Types Bool → Bool

# String.Join
This shard concatenates all the elements of a string sequence, using the specified separator between each element.

I/O Types [String Bytes] → String

Parameters
Separator String

# StringToBytes
Converts a string to its byte representation.

I/O Types String → Bytes

# GlobalOnce
Executes the shard or sequence of shards only once per mesh global execution.

I/O Types Any → Any

Parameters
Action Shard/[Shard]

# Math.Atan
This shard calculates the inverse tangent of the given input, where the input is the tangent value. The output is the angle in radians whose tangent is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Div
This shard divides the input value by the value provided in the Operand parameter.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.Log
This shard calculates the natural logarithm for the given input. The output is the exponent to which e must be raised to obtain the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# IsImage
Checks the input value if it is an Image. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# Atan
This shard calculates the inverse tangent of the given input, where the input is the tangent value. The output is the angle in radians whose tangent is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.TGamma
This shard calculates the gamma function of the given input. The gamma function is a mathematical function that extends the concept of factorial to non-integer and complex numbers.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Orthographic
This shard creates a 4x4 orthographic projection matrix based on the width size, height size, near, and far planes specified in the appropriate parameters. A 4x4 matrix is a sequence with exactly 4 float4 vectors while a float4 vector is a vector with 4 float elements.

I/O Types None → [Float4](4)

Parameters
Width Int/Float
Height Int/Float
Near Int/Float
Far Int/Float

# IsColor
Checks the input value if it is vector of four color channels (RGBA). The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# SwitchTo
Suspends the current Wire and switches execution to the specified Wire.

I/O Types Any → Any

Parameters
Wire Wire/String/None
Restart Bool

# Bytes.Join
This shard will concatenate a sequence of strings or bytes into a single string or byte array and output it as a byte array.

I/O Types [String Bytes] → Bytes

# ExpectSeq
Checks if the input value is a sequence; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Any]

# ExpectBool
Checks the input value if it is a Boolean. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Bool

# Profile
This shard outputs the amount of time it took to execute the shards provided in the Action parameter, automatically choosing the most appropriate time unit (ns, μs, ms, s).

I/O Types Any → Any

Parameters
Action Shard/[Shard]
Label String

# Math.Erf
This shard calculates the error function of the given input. The error function is related to the probability that a random variable with normal distribution of mean 0 and variance 1/2 falls in the range specified by the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Shards.Enumerate
Returns a sequence of all shard names in the system.

I/O Types None → [String]

Parameters
Category None/String

# BytesToString
Converts a sequence of bytes into a string. Each byte in the sequence is interpreted as a character in the resulting string.

I/O Types Bytes → String

# IsString
Checks the input value if it is a String. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# ExpectAudio
Checks the input value if it is an Audio file. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Audio

# Shuffle
Shuffles the elements of the sequence variable. Works only on sequences. If the variable is not a sequence, it simply passes through without failing.

I/O Types Any → Any

Parameters
Name String/Var(Any)
Key Any
Global Bool

# Math.AxisAngleX
This shard creates a rotation quaternion for rotation around the X-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

I/O Types Float → Float4

# IsNotNone
Gets whether the type of the input is different from `None`.

I/O Types Any → Bool

# Restart
Restarts the current flow with the provided input. This shard is used to restart the execution of the current wire from the beginning, using the same input. It ensures that the input type matches the wire's root input type. Note: This is a flow stopper and will not continue to subsequent shards in the current execution sequence.

I/O Types Any → None

# Clamp
This shard ensures the input value falls within the specified range. If the value falls below the minimum, the Min value is returned. If the value exceeds the maximum, the Max value is returned. Otherwise, the value is returned unchanged.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color

Parameters
Min Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])
Max Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.Cosh
This shard calculates the hyperbolic cosine of the given input, where the input is the real number. The hyperbolic cosine is a hyperbolic function that is analogous to the circular cosine function, but it uses exponential functions instead of angles.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# ExpectInt8
Checks the input value if it is a vector with eight Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Int8

# Math.Sqrt
This shard calculates the square root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# PauseMs
Pauses the wire for a given amount of time.

I/O Types Any → Any

Parameters
Time None/Int/Var(Int)

# Branch
Creates a branch from the specified Behavior and schedules all the Wires specified. Every time this shard is called, it will progress the state of all the Wires specified asynchronously and continue execution of the current Wire. This shard is like a mass Step, where it Steps all the Wires specified.

I/O Types Any → Any

Parameters
Wires Wire/[Wire]/None
FailureBehavior BranchFailure
CaptureAll Bool
Mesh None/Mesh

# Math.LookAt
This shard creates a 4x4 view matrix (a sequence of four float4 vectors) for a camera based on the camera's position and a target point which is represented as a table with two float3 vectors: 'Position' and 'Target', that is passed as input. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

I/O Types {Position: Float3 Target: Float3} → [Float4](4)

# ToFloat2
Converts various input types to a vector of two Float elements. If a single value or a collection with only one element is provided, the second element in the resulting vector will be set to 0.

I/O Types Any → Any

# Table
Creates an empty table. Useful to declare and specify types.

I/O Types Any → Any

Parameters
Name String/Var(Any)
Key Any
Global Bool
Type None/Type

# CaptureLog
Captures log messages based on specified parameters, such as the number of messages to retain, the minimum log level, and the log format pattern. It can optionally suspend execution until new log messages are available.

I/O Types None → [String]

Parameters
Size Int
MinLevel String
Pattern String
Suspend Bool

# IsInt16
Checks the input value if it is a vector of 16 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# Math.Lerp
Linearly interpolate between the start value specified in the `First` parameter and the end value specified in the `Second` parameter based on the factor provided as input.

I/O Types Float → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
First Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)
Second Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)

# Time.EpochMs
This shard outputs the the amount of time that has elapsed from the Unix epoch to the current system time in milliseconds.

I/O Types None → Int

# Math.Or
This shard performs a bitwise OR operation on the input value with the value specified in the Operand parameter and outputs the result. A bitwise OR operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if either bit is 1 and 0 if both bits are 0. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the Or comparison.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool → Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool

Parameters
Operand Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# Math.Log2
This shard calculates the base 2 logarithm for the given input. The output is the exponent to which 2 must be raised to obtain the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Cond
Takes a sequence of conditions and predicates. Evaluates each condition one by one and if one matches, executes the associated action.

I/O Types Any → Any

Parameters
Wires [Shard [Shard] None]
Passthrough Bool
Threading Bool

# Math.Negate
This shard reverses the sign of the input. (A positive number becomes negative, and vice versa).

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

# Expect
Checks if the input value matches the expected type specified by the 'Type' parameter. The shard outputs the input value unchanged if it is of the appropriate type, the shard will trigger an error, preventing further execution. The 'Unsafe' parameter can be set to skip deep type hashing and comparison to improve performance.

I/O Types Any → Any

Parameters
Type Type
Unsafe Bool

# SetLogLevel
This shard changes the log level to the level specified by the string passed as input. 

I/O Types String → Any

# Math.Cross
This shard computes the cross product of the float3 vector (or sequence of float3 vectors) provided as input and the float3 vector provided in the Operand parameter and outputs the result as a float3 vector (or sequence of float3 vectors). A float3 vector is a vector with 3 float elements.

I/O Types Float3/[Float3] → Float3/[Float3]

Parameters
Operand Float2/[Float2]/Float3/[Float3]/Float4/[Float4]/Var(Float2)/Var([Float2])/Var(Float3)/Var([Float3])/Var(Float4)/Var([Float4])

# Highest
Takes a sequence and outputs the element with the highest value.

I/O Types [Any] → Any

# Math.Log10
This shard calculates the base 10 logarithm for the given input. The output is the exponent to which 10 must be raised to obtain the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Erfc
This shard calculates the complementary error function of the given input. The complementary error function is related to the probability that the absolute value of a random variable with normal distribution of mean 0 and variance 1/2 is greater than the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Math.Slerp
This shard performs Spherical Linear Interpolation (SLERP) between two quaternions (represented as float4 vectors). It smoothly interpolates between the quaternions specified in the 'First' parameter and 'Second' parameter based on the input interpolation factor. A float4 vector is a vector with 4 float elements.

I/O Types Float → Float4

Parameters
First Float4/Var(Float4)
Second Float4/Var(Float4)

# Math.Translation
This shard creates a 4x4 translation matrix (a sequence of four float4 vectors) from a float3 vector input representing the translation in x, y, and z directions. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

I/O Types Float3 → [Float4](4)

# Math.Acosh
This shard calculates the inverse hyperbolic cosine of the given input, where the input is the hyperbolic cosine value. The output is the real number whose hyperbolic cosine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Match
Compares the input with the declared cases (in order of the declaration) and activates the shard of the first matched case.

I/O Types Any → Any

Parameters
Cases [Any]
Passthrough Bool

# String.Ends
This shard checks if the input string ends with the string specified in the With parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

I/O Types String → Bool

Parameters
With String/Var(String)

# ExpectBoolSeq
Checks if the input value is a sequence of Bools. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Bool]

Parameters
Unsafe Bool

# Recur
The Recur shard executes the Wire that calls it recursively, using the output of the Wire as input again, until the base cases are reached. It then combines the results to produce the final result. For the shard not to Recur endlessly, a base case needs to be defined, usually through a When or If shard.

I/O Types Any → Any

# Min
This shard compares the input with the value specified in the `Operand` parameter and outputs the smaller value.

I/O Types Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any] → Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any]

Parameters
Operand Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# IsWire
Checks the input value if it is a Wire. The shard will return true if the input is of the appropriate type, and false otherwise.

I/O Types Any → Bool

# ExpectFloatSeq
Checks if the input value is a sequence of Floats. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [Float]

Parameters
Unsafe Bool

# FastSqrt
This shard calculates the square root of the given input.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# IsInt
Checks the input value if it is of type Int. The shard will return true if the input value is of type Int, and false otherwise.

I/O Types Any → Bool

# Acos
This shard calculates the inverse cosine of the given input, where the input is the cosine value. The output is the angle in radians whose cosine is the input value.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# Do
Schedules and executes the specified Wire inline of the current Wire. The specified Wire needs to complete its execution before the current Wire continues its execution. This means that a pause in execution of the child Wire will also pause the parent Wire.

I/O Types Any → Any

Parameters
Wire Wire/String/None

# Erase
Deletes an index or indices from a sequence or a key or keys from a table.

I/O Types Any → Any

Parameters
Indices Any/Var(Any)
Name String/Var(Any)
Key Any
Global Bool

# AppendTo
Appends the input to the context variable passed to `:Collection`.

I/O Types Any → Any

Parameters
Collection Var([Any])/Var(String)/Var(Bytes)

# Time.DeltaMs
Outputs the time between the last call of this shard and the current call in milliseconds, capped to a limit

I/O Types None → Float

# Peek
Checks if another Wire has ended (Note that a looped Wire will never end). Outputs the Wire's output if it has ended, or none if it is still in progress.

I/O Types None → Any

Parameters
Wire Wire/String/None/Var(Wire)

# Math.QuatMultiply
This shard multiplies two quaternions (represented as float4 vectors) together. It combines the two rotations by multiplying the input quaternion with the operand quaternion. A float4 vector is a vector with 4 float elements.

I/O Types Float4 → Float4

Parameters
Operand Float4/Var(Float4)

# Reverse
This shard reverses the order of the elements in the input sequence or string.

I/O Types [Any]/String/Bytes → [Any]/String/Bytes

# ExpectBytes
Checks the input value if it is of type Bytes. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → Bytes

# Math.Expm1
This shard calculates the exponential function with base e (Euler's number) for the given input and subtracts 1 from the result.

I/O Types Float/Float2/Float3/Float4/Color/[Any] → Float/Float2/Float3/Float4/Color/[Any]

# ToBytes
This shard takes a value and converts it to a serialized binary representation (a serialized byte array).

I/O Types Any → Bytes

# ToAny
Converts an integer, bytes, or string value into its hexadecimal string representation.

I/O Types Any → Any

# Time.Now
This shard outputs the amount of time that has elapsed since the shards application or script was launched in seconds.

I/O Types None → Float

# ParseInt
Converts the string representation of a number to its signed integer equivalent.

I/O Types String → Int

Parameters
Base Int

# String.ToUpper
This shard converts all characters in the input string to uppercase.

I/O Types String → String

# Time.ToString
This shard converts time into a human readable string.

I/O Types Int/Float → String

Parameters
Millis Bool

# ExpectStringSeq
Checks if the input value is a sequence of Strings. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

I/O Types Any → [String]

Parameters
Unsafe Bool

# Enums reference

# WaitUntil

Policy for determining when to stop waiting in parallel operations. Defines the conditions under which execution should proceed.

### WaitUntil::FirstSuccess

Will wait until the first success and stop any other pending operation

### WaitUntil::AllSuccess

Will wait until all complete, will stop and fail on any failure

### WaitUntil::SomeSuccess

Will wait until all complete but won't fail if some of the wires failed

# Type

Fundamental data types supported by the system. Used for type checking and data manipulation.

### Type::None

### Type::Any

### Type::Bool

### Type::Int

### Type::Int2

### Type::Int3

### Type::Int4

### Type::Int8

### Type::Int16

### Type::Float

### Type::Float2

### Type::Float3

### Type::Float4

### Type::Color

### Type::Wire

### Type::Shard

### Type::Bytes

### Type::String

### Type::Image

### Type::Audio

# Mean

Type of mean calculation to be performed. Specifies whether to use arithmetic, geometric, or harmonic averaging.

### Mean::Arithmetic

### Mean::Geometric

### Mean::Harmonic

# RunWireMode

Execution mode for running wires. Specifies whether to run inline, asynchronously, or in a stepped manner.

### RunWireMode::Inline

### RunWireMode::Async

### RunWireMode::Stepped

# BranchFailure

Defines how to handle failures in branching operations. Determines whether to continue execution, handle known errors, or ignore failures entirely.

### BranchFailure::Everything

### BranchFailure::Known

### BranchFailure::Ignore

# LogLevel

Severity levels for logging messages. Helps categorize and filter log entries based on their importance.

### LogLevel::Trace

### LogLevel::Debug

### LogLevel::Info

### LogLevel::Warning

### LogLevel::Error

# Object types reference

# Mesh

**Thread safe**: false

