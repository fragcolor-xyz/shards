# Shards Reference

# Replace
This shard replaces all occurrences of the pattern(specified in the Patterns parameter) found in the input sequence or string, with replacements (specified in the Replacements parameter).

In: The input sequence or string to be modified. ([Any]/String)
Out: Outputs the resulting sequence or string with the replacements applied. ([Any]/String)

Params
Patterns: None/[String]/Var([String])/Var([Any])/[Any]
Replacements: None/Any/Var(Any)/[Any]/Var([Any])

# Regex.Replace
This shard modifies the input string by replacing all occurrences of the regex pattern, specified in the Regex parameter, with the replacement string specified in the Replacement parameter.

In: The string to modify. (String)
Out: The input string with all occurrences of the regex pattern replaced with the replacement string. (String)

Params
Regex: String
Replacement: String/Var(String)

# Math.Sinh
This shard calculates the hyperbolic sine of the given input, where the input is the real number. The hyperbolic sine is a hyperbolic function that is analogous to the circular sine function, but it uses exponential functions instead of angles.

In: The input float or sequence of floats to calculate the hyperbolic sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the hyperbolic sine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Floor
This shard rounds down the input to the nearest integer.

In: The input float or sequence of floats to round down. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input rounded down to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Pass
This shard is a "no operation" shard. It simply passes through the input without modifying it.

In: Any input type is accepted. The input value will pass through unchanged. (Any)
Out: Outputs the input value, passed through unchanged. (Any)

# ExpectIntSeq
Checks if the input value is a sequence of Ints. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Ints. ([Int])

Params
Unsafe: Bool

# String.Format
This shard concatenates all the elements of a sequence into a string

In: A sequence of values that will be converted to string and concatenated together. ([Any])
Out: A string consisting of all the elements of the sequence. (String)

# Detach
Schedules and executes the specified Wire asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Spawn, only one unique copy of the specified Wire can be scheduled using Detach. Future calls of Detach that schedules the same Wire will be ignored unless the specified Wire is Stopped or ends naturally.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: Outputs the input value, passed through unchanged. (Any)

Params
Wire: Wire/String/None
Restart: Bool

# ImageToFloats
Convert an image into a sequence of floats. Each pixel in the image is converted to a float value between 0 and 1 and stored in the sequence.

In: Takes an image as input. (Image)
Out: Outputs the input image represented as a seqeunce of floats. ([Float])

# IsAll
Checks if all elements in the input are equal to the given value. It outputs true if all elements are equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# Math.RShift
This shard shifts the bits of the input value to the right by the number of positions specified in the Operand parameter. The shard then outputs a value, whose binary representation is the resulting shifted binary.

In: The integer or the sequence of integers to shift the bits of. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any])
Out: Outputs the value resulting from the right shift operation. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any])

Params
Operand: Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

# IsAnyNot
Checks if any element in the input is not equal to the given value. It outputs true if any element is not equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is not equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# Asin (Math.Asin)
This shard calculates the inverse sine of the given input, where the input is the sine value. The output is the angle in radians whose sine is the input value.

In: The input float or sequence of floats to calculate the inverse sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose sine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Pow (Math.Pow)
This shard raises the input to the power of the exponent specified in the Operand parameter.

In: The base value to raise the power of. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: The result of raising the input to the power of the operand. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Memoize
Computes a value

In: (Any)
Out: (Any)

Params
Evaluate: Shard/[Shard]

# Dec (Math.Dec)
Decreases the input by 1.

In: The float or integer (or sequence of floats or integers) to decrease by 1. (Any)
Out: The input decreased by 1. (Any)

Params
Value: Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Time.EpochLocalMs
This shard outputs the amount of time that has elapsed from the Unix epoch to the current local system time in milliseconds.

In: The input of this shard is ignored. (None)
Out: Amount of time since the Unix epoch in local time milliseconds. (Int)

# Time.Epoch
This shard outputs the the amount of time that has elapsed from the Unix epoch to the current system time in seconds.

In: The input of this shard is ignored. (None)
Out: Amount of time since the Unix epoch in seconds. (Int)

# Time.Delta
Outputs the time between the last call of this shard and the current call in seconds, capped to a limit

In: The input of this shard is ignored. (None)
Out: Outputs the amount of time that has elapsed in seconds. (Float)

# Time.NowMs
This shard outputs the amount of time that has elapsed since the shards application or script was launched in milliseconds.

In: The input of this shard is ignored. (None)
Out: Outputs the amount of time that has elapsed in milliseconds. (Float)

# FlushLog
This shard flushes the log buffer to the console. This ensures that any pending log messages are immediately written to the console.

In: Any input type is accepted. The input value will pass through unchanged. (Any)
Out: Outputs the input value, passed through unchanged. (Any)

# Msg
Displays the passed message string to the user via standard output. The input variable is ignored, and only the static message is displayed.

In: The input is ignored. This shard displays a static message. (Any)
Out: The same variable that was inputted, unmodified. (Any)

Params
Message: String/Var(String)
Raw: Bool
Level: LogLevel/Var(LogLevel)
Name: String/Var(String)

# WhenDone
Schedules the specified Wire and runs it asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Detach, a copy of the specified Wire is scheduled every time the shard is called.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: Outputs the specific copy of the Wire that was scheduled. (Any)

Params
Wire: None/Wire/[Shard]

# Suspend
Pauses a specified Wire's execution. If no Wire is specified, pauses the current wire.

In: Any input type is accepted. The input value will pass through unchanged. (Any)
Out: Outputs the input value, passed through unchanged. (Any)

Params
Wire: Wire/String/None/Var(Wire)

# IsRunning
Checks if a Wire is running and outputs true if it is, false if otherwise. (Note that a looped Wire will always be running and thus will always return true)

In: The input of this shard is ignored. (None)
Out: This shard will either return true if the specified Wire is still running, or false if it has ended. (Bool)

Params
Wire: Wire/String/None/Var(Wire)

# DoMany
This shard takes a sequence of values as input, schedules multiple copies of a specified Wire and executes them sequentially. Each value from the sequence is provided as input to its corresponding copy of the specified Wire. The shard then outputs a sequence of values containing the output of all copies of the specified Wire.

In: This shard takes a sequence of values as input. Each value from the sequence is provided as input to its corresponding copy of the scheduled Wire. The total number of copies of the specified Wire scheduled, will be the same as the number of elements in the sequence provided. ([Any])
Out: This shard outputs the output of all the scheduled copies in a sequence. ([Any])

Params
Wire: None/Wire/[Shard]
ComposeSync: Bool

# Math.MatMul
Performs matrix multiplication on either two matrices or a matrix and a vector and outputs either a matrix or a vector accordingly. The two matrixes must be of similar dimensions (2x2, 3x3, or 4x4). And if multiplied with a vector, the vector too must have similar dimensions (2x2 with float2, 3x3 with float3, 4x4 with float4).

In: Takes a matrix as input. (2x2, 3x3 or 4x4) ([Float4](4)/[Float3](3)/[Float2](2))
Out: Outputs the result of the matrix multiplication. If a matrix is multiplied by a vector, the result is a vector (depending on the dimensions of the matrix provided). If two matrices are multiplied, the result is a matrix with the same dimensions as the input matrix. ([Float4](4)/Float4/[Float3](3)/Float3/[Float2](2)/Float2)

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Spawn
Schedules the specified Wire and runs it asynchronously. The current Wire will continue its execution independently of the specified Wire. Unlike Detach, a copy of the specified Wire is scheduled every time the shard is called.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: Outputs the specific copy of the Wire that was scheduled. (Wire)

Params
Wire: None/Wire/[Shard]

# ToHex
Converts an integer, bytes, or string value into its hexadecimal string representation.

In: Takes an integer, byte array, or string value. (Int/Int16/Bytes/String)
Out: The hexadecimal string representation of the input value. (String)

# TryMany
This shard takes a sequence of values as input, schedules multiple copies of a specified Wire and executes them asynchronously. Each value from the sequence is provided as input to its corresponding copy of the scheduled Wire. The shard will then wait for all the scheduled Wires to end, and then, depending on the Policy specified, the shard will either return the output of the first successful Wire, return a sequence with all the output from all the copies of the specified Wire or stop execution of the current Wire if all the copies failed.

In: This shard takes a sequence of values as input. Each value from the sequence is provided as input to its corresponding copy of the scheduled Wire. The total number of copies of the specified Wire scheduled, will be the same as the number of elements in the sequence provided. ([Any])
Out: Depending on the Policy specified the shard will return a different output. WaitUntil::FirstSuccess will return the output of the first successful Wire. WaitUntil::SomeSuccess return a sequence with all the output from all the copies of the specified Wire. WaitUntil::AllSuccess will either stop execution of the current Wire if any of the copies fail or return a sequence with all the output from all the copies of the specified Wire. ([Any])

Params
Wire: None/Wire/[Shard]
Policy: WaitUntil
Threads: Int

# Sin (Math.Sin)
This shard calculates the sine of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the sine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Step
The first time Step is called, the specified wire is scheduled. On subsequent calls, the specified Wire's state is progressed before the current Wire continues its execution. This means that a pause in execution of the child Wire will not pause the parent Wire.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: Outputs the input value, passed through unchanged. (Any)

Params
Wire: Wire/String/None

# TypeOf
Evaluates the output type of the given expression specified by the 'OutputOf' parameter and outputs that type. No input is required for this shard.

In: The input of this shard is ignored. (None)
Out: Outputs the type of the specified expression's output. (Type)

Params
OutputOf: Shard/[Shard]/None

# ExpectImage
Checks the input value if it is an Image. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Image. (Image)

# Stop
Either stops the execution of a specified Wire or the current Wire.

In: Any input type is accepted. The input value will either pass through unchanged or be ignored. (Any)
Out: Depending on what is specified in the Passthrough parameter, this shard either outputs the input value, passed through unchanged or it outputs the output of the stopped Wire. Note that if it outputs the output of the stopped wire, it will be of Type::Any and thus should be checked or converted to the appropriate Type. (Any)

Params
Wire: Wire/String/None/Var(Wire)
Passthrough: Bool

# Wait
Waits for the specified Wire to complete before resuming execution of the current Wire.

In: Any input type is accepted. The input value will either pass through unchanged or be ignored. (None)
Out: If Passthrough is true, this shard outputs the input value, passed through unchanged. Otherwise, it outputs the output of the Wire it waited for. (Any)

Params
Wire: Wire/String/None/Var(Wire)
Passthrough: Bool
Timeout: Float/Var(Float)/None

# String.FromCodePoints
Converts a sequence of integer codepoints into a string.

In: ([Int])
Out: (String)

# String.CodePoints
Converts a string into a sequence of integer codepoints.

In: (String)
Out: ([Int])

# String.Starts
This shard checks if the input string starts with the string specified in the With parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

In: The string to check. (String)
Out: True if the input string starts with the string specified, false otherwise. (Bool)

Params
With: String/Var(String)

# String.Split
This shard splits the input string into a sequence of its constituent strings, using the string specified in the Separator parameter to segment the input. If the KeepSeparator parameter is true, the separator will be included in the output.

In: The string to split. (String)
Out: A sequence of strings, containing the separated parts of the input string. ([String])

Params
Separator: String/Var(String)
KeepSeparator: Bool

# String.RFind
Finds the last occurence of the string specified in the String parameter in the input string and outputs the index of the first occurence.

In: The string to check. (String)
Out: The index of the first occurence of the string specified, or -1 if the string is not found. (Int)

Params
ToFind: String/Var(String)

# IsMore
Checks if the input is greater than the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is greater than the operand and false otherwise. (Bool)

Params
Value: Any

# String.Contains
This shard checks if the input string contains the string specified in the String parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

In: The string to check. (String)
Out: True if the input string contains the string specified, false otherwise. (Bool)

Params
String: String/Var(String)

# String.Trim
This shard removes all leading and trailing whitespace characters from the input string and outputs the trimmed string.

In: The string to trim. (String)
Out: The input string with all leading and trailing whitespace characters removed. (String)

# String.ToLower
This shard converts all characters in the input string to lowercase.

In: The string to convert to lowercase. (String)
Out: The input string converted to lowercase. (String)

# Regex.Match
This shard matches the entire input string against the regex pattern specified in the Regex parameter and outputs a sequence of strings, containing the fully matched string and any capture groups. It will return an empty sequence if there are no matches.

In: The string to match. (String)
Out: Outputs either a sequence of strings, containing the fully matched string and any capture groups or an empty sequence if there are no matches. ([String])

Params
Regex: String

# Regex.Search
This shard searches the input string for the regex pattern specified in the Regex parameter and outputs a sequence of strings, containing every occurrence of the pattern. An empty sequence is returned if there are no matches

In: The string to search. (String)
Out: A sequence of strings, each containing one occurrence of the regex pattern. ([String])

Params
Regex: String

# Zip
Zip will take any number of sequences and return a sequence of sequences, where each sequence is a tuple of the values from the input sequences at the same index.

In: (None)
Out: ([{Any}]/[[Any]])

Params
Sequences: [[Any] Var([Any])]
Keys: None/[String]

# IndexOf
This shard will search the input sequence for the index of an item or a pattern of items (specified in the Item parameter) and return its index(or a sequence of indices).

In: The sequence to search through. ([Any])
Out: The index of the item or a sequence of indices. ([Int]/Int)

Params
Item: Any
All: Bool
Predicate: Shard/[Shard]

# Flatten
This shard will take a sequence with nested values (eg. a sequence of sequences or a sequence of tables) and create a single sequence with all of values, nested values and keys as elements.

In: This shard will take a sequence or a table with nested values. (Any)
Out: This shard will return a single sequence with all of values, nested values and keys of the input as elements. (Any)

# Math.Percentile
This shard calculates the percentile of the input value within the specified sequence.

In: The sequence of floats to calculate the percentile of. ([Float])
Out: The percentile of the input value within the specified sequence. (Float)

Params
Percentile: Float/Var(Float)

# Lerp (Math.Lerp)
Linearly interpolate between the start value specified in the `First` parameter and the end value specified in the `Second` parameter based on the factor provided as input.

In: The factor to interpolate between the start and end values. (Float)
Out: The interpolated value between the start and end values based on the factor provided as input. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
First: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)
Second: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)

# Percentile (Math.Percentile)
This shard calculates the percentile of the input value within the specified sequence.

In: The sequence of floats to calculate the percentile of. ([Float])
Out: The percentile of the input value within the specified sequence. (Float)

Params
Percentile: Float/Var(Float)

# Math.Pow
This shard raises the input to the power of the exponent specified in the Operand parameter.

In: The base value to raise the power of. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: The result of raising the input to the power of the operand. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# WireComposer
Attempts to compose the specified wire and outputs "OK" if successful, or an error message if the composition fails.

In: The input of this shard is ignored. (None)
Out: Returns "OK" if the wire was successfully composed, otherwise returns an error message. (String)

Params
Wire: Wire/Var(Wire)

# Math.Dec
Decreases the input by 1.

In: The float or integer (or sequence of floats or integers) to decrease by 1. (Any)
Out: The input decreased by 1. (Any)

Params
Value: Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Inc (Math.Inc)
Increases the input by 1.

In: The float or integer (or sequence of floats or integers) to increase by 1. (Any)
Out: The input increased by 1. (Any)

Params
Value: Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# Math.Round
This shard rounds the input floating-point number to the nearest integer.

In: The input float or sequence of floats to round. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input rounded to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Math.Trunc
This shard truncates the input floating-point number towards zero, removing any fractional part without rounding.

In: The input float or sequence of floats to truncate. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input truncated to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Ceil (Math.Ceil)
This shard rounds up the input to the nearest integer.

In: The input float or sequence of floats to round up. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input rounded up to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Floor (Math.Floor)
This shard rounds down the input to the nearest integer.

In: The input float or sequence of floats to round down. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input rounded down to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Math.Ceil
This shard rounds up the input to the nearest integer.

In: The input float or sequence of floats to round up. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the input rounded up to the nearest integer (as a float). (Float/Float2/Float3/Float4/Color/[Any])

# Math.LGamma
This shard calculates the log gamma function of the given input. The log gamma function is the natural logarithm of the absolute value of the gamma function.

In: The input float or sequence of floats to calculate the log gamma function of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the log gamma function of the input. The output is always positive for positive inputs. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Asinh
This shard calculates the inverse hyperbolic sine of the given input, where the input is the hyperbolic sine value. The output is the real number whose hyperbolic sine is the input value.

In: The input float or sequence of floats to calculate the inverse hyperbolic sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the real number whose hyperbolic sine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Log1p
This shard adds 1 to the input and then calculates the natural logarithm of the result.

In: The input float or sequence of floats to add 1 to and then calculate the natural logarithm of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the natural logarithm of the input plus 1. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Tanh
This shard calculates the hyperbolic tangent of the given input, where the input is the real number. The hyperbolic tangent is a hyperbolic function that is analogous to the circular tangent function, but it uses exponential functions instead of angles.

In: The input float or sequence of floats to calculate the hyperbolic tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the hyperbolic tangent of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Asin
This shard calculates the inverse sine of the given input, where the input is the sine value. The output is the angle in radians whose sine is the input value.

In: The input float or sequence of floats to calculate the inverse sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose sine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Tan
This shard calculates the tangent of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the tangent of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Sin
This shard calculates the sine of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the sine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the sine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# FastInvSqrt (Math.FastInvSqrt)
This shard calculates the inverse square root of the given input.

In: The input float or sequence of floats to calculate the inverse square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the inverse square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.FastInvSqrt
This shard calculates the inverse square root of the given input.

In: The input float or sequence of floats to calculate the inverse square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the inverse square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.FastSqrt
This shard calculates the square root of the given input.

In: The input float or sequence of floats to calculate the square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Sqrt (Math.Sqrt)
This shard calculates the square root of the given input.

In: The input float or sequence of floats to calculate the square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# IntsToBytes
Converts a sequence of integers into a byte array. Each integer in the sequence is serialized into its binary representation and concatenated into the resulting byte array.

In: Accepts sequence of integers as input. ([Int])
Out: A byte array representing the sequence of integers. (Bytes)

# HexToBytes
Converts a hexadecimal string to its original byte array representation.

In: Accepts a hexadecimal string as input. The input may optionally start with '0x' or '0X'. (String)
Out: The decoded byte array from the input hexadecimal string. (Bytes)

# Math.Exp2
This shard calculates the exponential function with base 2 for the given input. The exponential function with base 2 is equivalent to raising 2 to the power of the input.

In: The input float or sequence of floats used as the exponent for the base 2 exponential function. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the result of the exponential operation. (Float/Float2/Float3/Float4/Color/[Any])

# Fail
Stops the current flow and cancels the execution with the provided error message. This shard is used to signal an error and halt the execution of the current wire.

In: The error message to cancel the flow with. (String)
Out: This shard does not produce an output as it cancels the flow. (None)

# Math.Exp
This shard calculates the exponential function with base e (Euler's number) for the given input. The exponential function is equivalent to raising Euler's number to the power of the input.

In: The input float or sequence of floats to use as the exponent for the base e exponential function. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the result of the exponential operation. (Float/Float2/Float3/Float4/Color/[Any])

# Abs (Math.Abs)
This shard outputs the absolute value of the input.

In: The numeric value or a sequence of numeric values to get the absolute value of. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the absolute value of the input. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

# Math.Abs
This shard outputs the absolute value of the input.

In: The numeric value or a sequence of numeric values to get the absolute value of. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the absolute value of the input. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

# Math.LShift
This shard shifts the bits of the input value to the left by the number of positions specified in the Operand parameter. The shard then outputs a value, whose binary representation is the resulting shifted binary.

In: The integer or the sequence of integers to shift the bits of. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any])
Out: Outputs the value resulting from the left shift operation. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any])

Params
Operand: Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]

# Math.Divide
This shard divides the input value by the value provided in the Operand parameter.

In: The value or the sequence of values to divide the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the division. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Sub (Math.Subtract)
This shard subtracts the value provided in the Operand parameter from the input value.

In: The value or the sequence of values to subtract the value specified in the Operand parameter from. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the subtraction. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Add (Math.Add)
This shard adds the input value to the value provided in the Operand parameter.

In: The value or the sequence of values to add the value specified in the Operand parameter to. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the addition. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.QuatRotate
This shard rotates the input 3D vector (represented as a float3) by the quaternion (represented as a float4) specified in the Operand parameter and outputs the resulting rotated 3D vector. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

In: Takes a float3 vector representing the 3D vector to be rotated. (Float3)
Out: Outputs a float3 vector representing the rotated 3D vector. (Float3)

Params
Operand: Float4/Var(Float4)

# Math.Atanh
This shard calculates the inverse hyperbolic tangent of the given input (atanh(x)), where x, outputs y such that tanh(y) = x.

In: The input float or sequence of floats to calculate the inverse hyperbolic tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the real number whose hyperbolic tangent is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Time.MovingAverage
This shard computes the average of a floating point number over a specified number of frames.

In: The floating point number to compute the average of. (Float)
Out: The average of the floating point number over the specified number of frames. (Float)

Params
Window: Int
Clear: None/Var(Bool)

# Resume
Resumes another Wire (previously paused using Suspend).

In: Any input type is accepted. The input value will pass through unchanged. (Any)
Out: Outputs the input value, passed through unchanged. (Any)

Params
Wire: Wire/String/None/Var(Wire)

# Extend
Extends the mutable sequence parameter with the elements of the input sequence.

In: The sequence to be appended to the target sequence. ([Any])
Out: The input sequence (pass-through). ([Any])

Params
Target: Var([Any])

# Math.Unproject
This shard converts 2D screen coordinates back to 3D world coordinates using the inverse of a view-projection matrix. Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 float elements).It performs the reverse operation of the projection pipeline, including inverse matrix multiplication, and coordinate space transformations using the 4x4 view-projection matrix specified in the Matrix parameter and the screen size in the ScreenSize parameter.

In: Takes a float3 vector representing the 3D vector where x and y are screen coordinates, and z is the depth value in screen space. (Float3)
Out: Outputs a float3 vector representing the unprojected 3D point in world space. (Float3)

Params
Matrix: [Float4](4)/Var([Float4](4))
ScreenSize: Float2/Var(Float2)
DepthRange: None/Float2/Var(Float2)
FlipY: None/Bool/Var(Var(Bool))

# Math.Project
This shard converts the input 3D world coordinates to 2D screen coordinates using a view-projection matrix. Both 3D and 2D coordinates are represented as float3 vectors (vectors with 3 float elements).It performs the full projection pipeline including matrix multiplication, perspective division, and viewport transformation using the 4x4 view-projection matrix specified in the Matrix parameter and the screen size in the ScreenSize parameter.

In: Takes a float3 vector representing the 3D point in world space where x, y, and z are the coordinates in world space. (Float3)
Out: Outputs a float3 vector representing the projected 2D point (x, y) in screen space, with the z component representing the depth. (Float3)

Params
Matrix: [Float4](4)/Var([Float4](4))
ScreenSize: Float2/Var(Float2)
FlipY: Bool/Var(Var(Bool))

# Math.MatIdentity
This shard creates a standard 4x4 identity matrix. The standard identity matrix is a square matrix with 1s on the main diagonal and 0s for the other elements. A 4x4 matrix is a sequence with exactly 4 float4 vector and a float4 vector is a vector with 4 float elements.

In: The input of this shard is ignored. (None)
Out: Outputs a 4x4 identity matrix (a sequence of four float4 vectors). The matrix will have 1s on the main diagonal and 0s for the other elements. ([Float4](4))

Params
Type: Type

# DegreesToRadians (Math.DegreesToRadians)
This shard converts the input angle from degrees to radians. The conversion is done using the formula: radians = degrees * (π / 180).

In: Takes a float value representing an angle in degrees. (Float)
Out: Outputs a float value representing the input angle in radians. (Float)

# Math.DegreesToRadians
This shard converts the input angle from degrees to radians. The conversion is done using the formula: radians = degrees * (π / 180).

In: Takes a float value representing an angle in degrees. (Float)
Out: Outputs a float value representing the input angle in radians. (Float)

# Math.AxisAngleZ
This shard creates a rotation quaternion for rotation around the Z-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

In: Takes a float value representing the Z rotation in radians. (Float)
Out: Outputs a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the Z-axis. (Float4)

# Math.AxisAngleY
This shard creates a rotation quaternion for rotation around the Y-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

In: Takes a float value representing the Y rotation in radians. (Float)
Out: Outputs a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the Y-axis. (Float4)

# Tan (Math.Tan)
This shard calculates the tangent of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the tangent of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Rotation
This shard creates a 4x4 rotation matrix (a sequence of four float4 vectors) from a float4 vector input representing a rotation quaternion. A float4 vector is a vector with 4 float elements.

In: Takes a float4 vector (a vector with 4 float elements) representing a rotation quaternion. (Float4)
Out: Outputs a 4x4 rotation matrix (a sequence of four float4 vectors). ([Float4](4))

# Math.And
This shard performs a bitwise AND operation on the input value with the value specified in the Operand parameter and outputs the result. A bitwise AND operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if the bits are 1 and 0 otherwise. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the AND comparison.

In: The value or the sequence of values to compare the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)
Out: This shard outputs the value resulting from the AND operation. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)

Params
Operand: Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# Math.Scaling
This shard creates a 4x4 scaling matrix (a sequence of four float4 vectors) from a float3 vector input that represents the scaling factors in x, y, and z directions. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

In: Takes a float3 vector (a vector with 3 float elements) that represents the scaling factors in x, y, and z directions. The first element in the vector being x, the second y and the third z. (Float3)
Out: Outputs a 4x4 scaling matrix (a sequence of four float4 vectors). ([Float4](4))

# Math.Inverse
This shard takes a 4x4 matrices as input and computes its inverse. A 4x4 matrix is a sequence with exactly 4 float4 vectors while a float4 vector is a vector with 4 float elements.

In: Takes a 4x4 matrix (a sequence of four float4 vectors) as input. ([Float4](4))
Out: Outputs the inverse of the input 4x4 matrix. ([Float4](4))

# Math.Transpose
Performs matrix transposition on the input matrix. Transposition flips the matrix over its main diagonal, switching its rows and columns. This shard supports 2x2, 3x3, and 4x4 as input matrices. A 4x4 matrix is a sequence with exactly 4 float4 vectors, a 3x3 matrix is a sequence with exactly 3 float3 vectors, and a 2x2 matrix is a sequence with exactly 2 float2 vectors.

In: Takes a matrix (sequence of float2, float3, or float4 vectors) as input. ([Float4](4)/[Float3](3)/[Float2](2))
Out: Outputs the transposed the matrix. ([Float4](4)/[Float3](3)/[Float2](2))

# Math.Length
Computes the magnitude of a float vector of any dimension and outputs the result as a float.

In: Accepts a float vector of any dimension (e.g., float2, float3, float4). (Float2/[Float2]/Float3/[Float3]/Float4/[Float4])
Out: Outputs the magnitude of the input vector as a float. (Float)

# Math.LengthSquared
Computes the squared magnitude of a float vector of any dimension and outputs the result as a float.

In: Accepts a float vector of any dimension (e.g., float2, float3, float4). (Float2/[Float2]/Float3/[Float3]/Float4/[Float4])
Out: Outputs the squared magnitude of the input vector as a float. (Float)

# IntRange
Outputs a sequence of integers from Start (inclusive) to End (inclusive).

In: Input is ignored (None)
Out: Sequence of integers ([Int])

Params
Start: Int/Var(Int)
End: Int/Var(Int)

# Math.Normalize
This shard normalizes a float vector of any dimension or a sequence of floats, scaling it to have a magnitude of 1 while preserving its direction. By default, output values can range from -1.0 to 1.0. If the 'Positive' parameter is set to true, the output will be scaled to the range 0.0 to 1.0. For example, normalizing [4.0 -5.0 6.0 -7.0] will result in [0.3563, -0.4454, 0.5345, -0.6236], which has a length of 1. 

In: Accepts a float vector of any dimension (e.g., float2, float3, float4) or a float sequence of any length. ([Float]/Float2/[Float2]/Float3/[Float3]/Float4/[Float4])
Out: Outputs a float vector of the same dimension or a float sequence of the same length as what was passed as input but with its values normalized to a magnitude of 1. ([Float]/Float2/[Float2]/Float3/[Float3]/Float4/[Float4])

Params
Positive: Bool

# Math.Dot
Computes the dot product of two float vectors with an equal number of elements, and outputs the resulting float value. The first float vector is passed as input and the second float vector is specified in the Operand parameter.

In: Takes in a float vector of any dimension (e.g., float2, float3, float4). (Float2/[Float2]/Float3/[Float3]/Float4/[Float4])
Out: Outputs the resulting dot product as a float value. (Float2/[Float2]/Float3/[Float3]/Float4/[Float4])

Params
Operand: Float2/[Float2]/Float3/[Float3]/Float4/[Float4]/Var(Float2)/Var([Float2])/Var(Float3)/Var([Float3])/Var(Float4)/Var([Float4])

# Math.Xor
This shard performs a bitwise XOR operation on the input with the value specified in the Operand parameter and outputs the result. A bitwise XOR operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if the bits are different and 0 if they are the same. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the XOR comparison.

In: The value or the sequence of values to compare the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)
Out: This shard outputs the value resulting from the XOR operation. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)

Params
Operand: Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# If
Evaluates a predicate and executes an action.

In: The value that will be passed to the predicate. (Any)
Out: The input of the shard if `Passthrough` is `true`; otherwise, the output of the action that was performed (i.e. `Then` or `Else`). (Any)

Params
Predicate: Shard/[Shard]/None
Then: Shard/[Shard]/None
Else: Shard/[Shard]/None
Passthrough: Bool

# RLimit
This shard truncates the input sequence to the last specified number of elements (Max) and outputs the truncated sequence. If Max is set to 1, it outputs a single element.

In: The input sequence to truncate. ([Any])
Out: The truncated sequence containing the last 'Max' elements, or a single element if Max is 1. (Any)

Params
Max: Int

# Max
This shard compares the input with the value specified in the `Operand` parameter and outputs the larger value.

In: The first value to compare with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: The larger value between the input and the operand. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# When
Conditional shard that only executes the action if the predicate is true.

In: The value that will be passed to the predicate. (Any)
Out: The input of the shard if `Passthrough` is `true`, or the `Predicate` is `false`; otherwise, the output of the `Action`. (Any)

Params
Predicate: Shard/[Shard]/None
Action: Shard/[Shard]/None
Passthrough: Bool

# Maybe
Attempts to activate a shard or a sequence of shards. Upon failure, activate another shard or sequence of shards.

In: Must match the input types of the first shard in the sequence. (Any)
Out: Will match the output types of the first shard of the sequence. (Any)

Params
Shards: Shard/[Shard]/None
Else: Shard/[Shard]/None
Silent: Bool

# FromBase64
Decodes a Base64 encoded string to its original byte representation.

In: A Base64 encoded string to be decoded. (String)
Out: The decoded bytes from the input Base64 string. (Bytes)

# ToBase64
Encodes the input bytes or string value to its Base64 string representation.

In: Accepts a byte array or a string value as input. (Bytes/String)
Out: Outputs the Base64 encoded string representation of the input value. (String)

# BytesToAudio
Converts a byte array containing float samples back into an audio buffer.

In: Accepts a byte array containing float samples. (Bytes)
Out: Returns the constructed audio buffer. (Audio)

Params
Channels: Int
SampleRate: Int

# AudioToBytes
Converts an audio buffer into a byte array.

In: Accepts an audio buffer as input. (Audio)
Out: The input audio buffer represented as a byte array. (Bytes)

# ImageToBytes
Converts an image into a byte array.

In: Accepts an image as input. (Image)
Out: The input image represented as a byte array. (Bytes)

# BytesToInts
Convert bytes into a sequence of integers. Each byte is interpreted as an integer and stored in the sequence.

In: Takes a byte array as input. (Bytes)
Out: Outputs the input bytes represented as a sequence of integers. ([Int])

# ExpectInt3
Checks the input value if it is a vector with three Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int3. (Int3)

# Math.Cbrt
This shard calculates the cube root of the given input.

In: The input float or sequence of floats to calculate the cube root of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the cube root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# IsTable
Checks the input value if it is a Table. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Table, and false otherwise. (Bool)

# WhenNot
Conditional shard that only executes the action if the predicate is false.

In: The value that will be passed to the predicate. (Any)
Out: The input of the shard if `Passthrough` is `true`, or the `Predicate` is `true`; otherwise, the output of the `Action`. (Any)

Params
Predicate: Shard/[Shard]/None
Action: Shard/[Shard]/None
Passthrough: Bool

# IsSeq
Checks the input value if it is of the type specified. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of the specified type, and false otherwise. (Bool)

# IsBool
Checks the input value if it is a Boolean. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Bool, and false otherwise. (Bool)

# IsAnyLessEqual
Checks if any element in the input is less than or equal to the given value. It outputs true if any element is less or equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is less than or equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# IsBytes
Checks the input value if it is of type Bytes. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Bytes, and false otherwise. (Bool)

# IsFloat3
Checks the input value if it is a vector with three Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Float3, and false otherwise. (Bool)

# IsFloat2
Checks the input value if it is a vector with two Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Float2, and false otherwise. (Bool)

# IsFloat
Checks the input value if it is of type Float. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Float, and false otherwise. (Bool)

# IsInt8
Checks the input value if it is a vector of 8 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int8, and false otherwise. (Bool)

# IsInt4
Checks the input value if it is a vector of 4 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int4, and false otherwise. (Bool)

# IsInt3
Checks the input value if it is a vector of 3 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int3, and false otherwise. (Bool)

# IsInt2
Checks the input value if it is a vector of 2 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int2, and false otherwise. (Bool)

# Math.Mod
This shard calculates the remainder of the division of the input value by the value provided in the Operand parameter.

In: The value or the sequence of values to divide the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the modulus operation. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# ExpectAudioSeq
Checks if the input value is a sequence of Audio buffers. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Audio data. ([Audio])

Params
Unsafe: Bool

# ExpectWireSeq
Checks if the input value is a sequence of Wires. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Wires. ([Wire])

Params
Unsafe: Bool

# ExpectColorSeq
Checks if the input value is a sequence of Color vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Color vectors. ([Color])

Params
Unsafe: Bool

# ExpectImageSeq
Checks if the input value is a sequence of Images. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Images. ([Image])

Params
Unsafe: Bool

# ExpectBytesSeq
Checks if the input value is a sequence of Bytes. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Bytes. ([Bytes])

Params
Unsafe: Bool

# ExpectInt16Seq
Checks if the input value is a sequence of Int16 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Int16 vectors. ([Int16])

Params
Unsafe: Bool

# ExpectInt8Seq
Checks if the input value is a sequence of Int8 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Int8 vectors. ([Int8])

Params
Unsafe: Bool

# RadiansToDegrees (Math.RadiansToDegrees)
This shard converts the input angle from radians to degrees. The conversion is done using the formula: degrees = radians * (180 / π).

In: Takes a float value representing an angle in radians. (Float)
Out: Outputs a float value representing the input angle in degrees. (Float)

# ExpectInt3Seq
Checks if the input value is a sequence of Int3 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Int3 vectors. ([Int3])

Params
Unsafe: Bool

# ExpectInt2Seq
Checks if the input value is a sequence of Int2 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Int2 vectors. ([Int2])

Params
Unsafe: Bool

# ExpectFloat4Seq
Checks if the input value is a sequence of Float4 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Floats4 vectors. ([Float4])

Params
Unsafe: Bool

# Hash
This shard takes any input type, hashes them using the XXH128 hashing algorithm and outputs their 128-bit hash value as an int2 (a sequence with 2 integers as elements).

In: Input of any type is accepted. (Any)
Out: This shard outputs the input's hashed value as an int2 (a sequence with 2 integers as elements) with 64-bit integer elements. (Int2)

# ExpectFloat3Seq
Checks if the input value is a sequence of Float3 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Float3 vectors. ([Float3])

Params
Unsafe: Bool

# WireRunner
Runs the wire variable specified by the input wire variable.

In: (Any)
Out: (Any)

Params
Wire: Wire/Var(Wire)
Mode: RunWireMode

# ExpectWire
Checks the input value if it is a Wire. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Wire. (Wire)

# Expand
Schedules (n) number of copies of the specified Wire, where (n) is the number specified in the Size parameter. The parent Wire will wait until all the scheduled copies have ended and will either return a sequence of values outputs of all the copied Wires or the output of the first Wire that succeeds. Once done, it will continue with its own execution.

In: This shard takes a value of any type as input. This value is provided as input to every scheduled copy of the specified Wire. (Any)
Out: Depending on the Policy specified the shard will return a different output. WaitUntil::FirstSuccess will return the output of the first successful Wire. WaitUntil::SomeSuccess return a sequence with all the output from all the copies of the specified Wire. WaitUntil::FirstSuccess will either stop execution of the current Wire if any of the copies fail or return a sequence with all the output from all the copies of the specified Wire. ([Any])

Params
Size: Int
Wire: None/Wire/[Shard]
Policy: WaitUntil
Threads: Int

# Clear
Clears all elements from the sequence or table passed to it. Applicable only to sequences and tables. For sequences, this operation is very fast as Shards recycles memory extensively. If the variable does not exist or the type is not a sequence or table, it simply passes through without failing.

In: Any input is ignored. (Any)
Out: The input is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# ExpectColor
Checks the input value if it is vector of four color channels (RGBA). The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Color. (Color)

# ExpectString
Checks the input value if it is a String. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type String. (String)

# ExpectFloat4
Checks the input value if it is a vector with float Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Float4. (Float4)

# ExpectFloat2
Checks the input value if it is a vector with two Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Float2. (Float2)

# ExpectFloat
Checks the input value if it is of type Float. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, it will fail.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Float. (Float)

# ExpectInt16
Checks the input value if it is a vector with sixteen Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int16 (Int16)

# Math.Acos
This shard calculates the inverse cosine of the given input, where the input is the cosine value. The output is the angle in radians whose cosine is the input value.

In: The input float or sequence of floats to calculate the inverse cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose cosine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# ExpectInt4
Checks the input value if it is a vector with four Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int4. (Int4)

# ExpectInt2
Checks the input value if it is a vector with two Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int2. (Int2)

# ExpectInt
Checks the input value if it is of type Int. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int. (Int)

# BitSwap64
This shard takes a 64-bit integer, reverses their order of its bytes, and outputs the result as an integer. This is useful for converting between different endianness formats.

In: Takes a 64-bit integer value. (Int)
Out: Outputs the reversed bytes as an integer. (Int)

# BitSwap32
This shard takes a 32-bit integer, reverses their order of its bytes, and outputs the result as an integer. This is useful for converting between different endianness formats.

In: Takes a 32-bit integer value. (Int)
Out: Outputs the reversed bytes as an integer. (Int)

# Math.Compose
Creates a 4x4 transformation matrix (sequence of four float4 vectors) from a table containing the appropriate Translation, Rotation and Scale values. values. The translation value should be a float3 vector representing positions on the x y z axis. The rotation value should be a float4 vector representing the quaternion rotation. Lastly, the scale should be a float3 vector representing the size on the x y and z axis. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements.

In: Takes a table as input. The table should have a Translation key with a float3 vector value, a Rotation key with a float4 vector value and a Scale key with a float3 vector value. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} ({translation: Float3 rotation: Float4 scale: Float3})
Out: Outputs a 4x4 transformation matrix (sequence of four float4 vectors) that combines the input translation, rotation, and scale. ([Float4](4))

# ExpectNone
Checks the input value if it is none. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type None. (None)

# ToFloat3
Converts various input types to a vector of three Float elements. If a single value or a collection with less than 3 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of three Float elements. (Any)

# Last
Returns the last element from a sorted table or sequence. For tables, returns a [key, value] pair. Returns None if empty. Note: This operation is fast but unsafe unless the output is cloned (using Set instead of Ref) when combined with await or suspended wire flow.

In: A table or sequence to get the last element from. ([Any]/{Any})
Out: For sequences: the last value. For tables: a [key, value] pair. Returns None if input is empty. (Any)

# ToInt16
Converts various input types to a vector of sixteen Int elements. If a single value or a collection with less than 16 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of sixteen Int elements. (Any)

# Mul (Math.Multiply)
This shard multiplies the input value by the value provided in the Operand parameter.

In: The value or the sequence of values to multiply the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the multiplication. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# ToInt8
Converts various input types to a vector of eight Int elements. If a single value or a collection with less than 8 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of eight Int elements. (Any)

# Await
Executes a shard or a sequence of shards asynchronously and awaits their completion.

In: Must match the input types of the first shard in the sequence. (Any)
Out: Will match the output types of the first shard of the sequence. (Any)

Params
Shards: Shard/[Shard]/None

# ToInt3
Converts various input types to a vector of three Int elements. If a single value or a collection with less than 3 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of three Int elements. (Any)

# ToInt2
Converts various input types to a vector of two Int elements. If a single value or a collection with only one element is provided, the second element in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of two Int elements. (Any)

# Iterate
Searches through a sorted table input for a range of matching elements. Returns all values from the table that have keys between the From and To keys.

In: The input table to perform the range query on. ({Any})
Out: Passes through the input table unchanged. ({Any})

Params
From: Any
To: Any/None
Action: Shard/[Shard]/None

# Shards.EnumTypeHelp
Returns a table of help information for the enum type specified by the input id.

In: (Int)
Out: ({Any})

# Not
Computes the logical negation of the input.

In: The value to be negated. (Bool)
Out: The negation of the input. (Bool)

# Shards.ObjectTypes
Returns a sequence of all object types in the system.

In: (None)
Out: ([Int])

# String.Find
Finds the next occurence of the string specified in the String parameter in the input string and outputs the index of the first occurence.

In: The string to check. (String)
Out: The index of the first occurence of the string specified, or -1 if the string is not found. (Int)

Params
ToFind: String/Var(String)

# ParseFloat
Converts the string representation of a number to its floating-point number equivalent.

In: A string representing a number. (String)
Out: A floating-point number equivalent to the number contained in the string input. (Float)

# NaNTo0
Replaces NaN (Not a Number) values in the input with 0. This shard can handle both single float values and sequences of float values.

In: A float value or a sequence of float values to be checked for NaN. (Float/[Float])
Out: The input with any NaN values replaced by 0. (Float/[Float])

# Browse
This shard will open the URL string input in the current system's default web browser.

In: The URL to navigate to. (String)
Out: Outputs the input value, passed through unchanged. (String)

# Math.Inc
Increases the input by 1.

In: The float or integer (or sequence of floats or integers) to increase by 1. (Any)
Out: The input increased by 1. (Any)

Params
Value: Var(Int)/Var(Int2)/Var(Int3)/Var(Int4)/Var(Int8)/Var(Int16)/Var(Float)/Var(Float2)/Var(Float3)/Var(Float4)/Var(Color)/Var([Any])

# ToString
Converts any input value to its string representation.

In: Input of any type is accepted. (Any)
Out: The string representation of the input value. (String)

# Lowest
Takes a sequence and outputs the element with the lowest value.

In: A sequence of elements of any type. ([Any])
Out: Outputs the element with the lowest value. (Any)

# Insert
Prepends the input to the context variable passed to `Collection`.

In: The value to prepend to the collection. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Index: Int/Var(Int)
Collection: Var([Any])/Var(String)/Var(Bytes)

# Once
Executes the shard or sequence of shards with the desired frequency in a wire flow execution.

In: (Any)
Out: (Any)

Params
Action: Shard/[Shard]
Every: Float/Var(Float)

# Fold
Folds a sequence into a single value by applying an operation (specified in the Apply parameter) to each item of the sequence. The operation can transform the type. Note that this shard is able to use the $0 internal variable for the accumulated value, $1 for the current item, and $i for the current index.

In: The sequence to fold. ([Any])
Out: The resulting value after folding the sequence. (Any)

Params
Apply: Shard/[Shard]
Initial: Any/Var(Any)

# Map
Processes each element of a sequence or key-value pair of a table using the shards specified in the `Apply` parameter and outputs the modified sequence or table. Note that this shard is able to use the $0 and $1 internal variables, as well as $i for the current index.

In: The sequence or table to process. ([Any]/{Any})
Out: The resulting processed sequence or table. ([Any])

Params
Apply: Shard/[Shard]

# Math.Not
This shard performs a bitwise NOT operation on the input. It flips all the bits of the input number.

In: The integer (or sequence of integers) to perform bitwise NOT on. (Int/Int2/Int3/Int4/Int8/Int16/[Any])
Out: The result of the bitwise NOT operation. (Int/Int2/Int3/Int4/Int8/Int16/[Any])

# FloatsToImage
Converts a sequence of floats into an image. The image dimensions (width and height) and the number of channels are specified by the appropriate parameters.

In: Takes a sequence of floats as input and converts it into an image. The sequence length must be equal to Width x Height x Channels. ([Float])
Out: This shard outputs and image. (Image)

Params
Width: Int
Height: Int
Channels: Int

# ExpectFloat3
Checks the input value if it is a vector with three Float elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Float3. (Float3)

# ExpectLike
Checks if the input value matches the type of the value provided in the TypeOf parameter or the output type of the given expression in the OutputOf parameter. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution. Note that it can only compare with either one of the parameters, not both; an error will be thrown if both are provided. The 'Unsafe' parameter can be set to skip deep type hashing and comparison to improve performance.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it matches the expected type. (Any)

Params
TypeOf: Any
OutputOf: Shard/[Shard]/None
Unsafe: Bool

# IsAllLessEqual
Checks if all elements in the input are less than or equal to the given value. It outputs true if all elements are less or equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are less than or equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# Pop
Pops (removes and outputs) the last element of the sequence variable. Works only on sequences. If the variable is not a sequence or the sequence is empty, an error is thrown.

In: Any input is ignored. (None)
Out: The element popped from the sequence. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# IsTrue
Gets whether the input is `true`.

In: The value to check against. (Bool)
Out: `true` if the input is `true`; otherwise, `false`. (Bool)

# IsAllMoreEqual
Checks if all elements in the input are greater than or equal to the given value. It outputs true if all elements are greater or equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are greater than or equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# Log
Logs the output of a shard or the value of a variable to the console along with an optional prefix string. The logging level can be specified to control the verbosity of the log output.

In: The value to be logged to the console. (Any)
Out: The same value that was inputted, unmodified. (Any)

Params
Prefix: String
Level: LogLevel/Var(LogLevel)
Name: String/Var(String)

# IsAnyMoreEqual
Checks if any element in the input is greater than or equal to the given value. It outputs true if any element is greater or equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is greater than or equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# IsAllMore
Checks if all elements in the input are greater than the given value. It outputs true if all elements are greater and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are greater than the specified value and false otherwise. (Bool)

Params
Value: Any

# IsAnyMore
Checks if any element in the input is greater than the given value. It outputs true if any element is greater and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is greater than the specified value and false otherwise. (Bool)

Params
Value: Any

# Shards.ObjectTypeHelp
Returns a table of help information for the object type specified by the input id.

In: (Int)
Out: ({Any})

# RTake
Works exactly like `Take` except that the selection indices are counted backwards from the last element in the target sequence. Also, `RTake` works only on sequences, not on tables.

In: The sequence from which elements will be extracted. (Bytes/String/[Any])
Out: The extracted elements. (Any)

Params
Indices: Int/[Int]/Var(Int)/Var([Int])

# FromBytes
This shard takes a serialized binary representation of a value and convert it back to its original type.

In: This shard will take a byte array. (Bytes)
Out: This shard will return the original value converted back to its original type. (Any)

# ToColor
Converts various input types to a vector of four color channels (RGBA). If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of four color channels (RGBA). (Any)

# And
If the input of the preceding shard is true, the flow continues; otherwise, the flow stops. This shard is typically used within conditional flows (e.g., If, When) to chain conditions. Note: Outside a conditional flow, it might restart the current wire, which can be used as a trick in certain scenarios.

In: If true, the flow continues; otherwise, it stops. (Bool)
Out: The output of this shard will be the input of the current conditional flow or wire. (Bool)

# IsAny
Checks if any element in the input is equal to the given value. It outputs true if any element is equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# IsLessEqual
Checks if the input is less than or equal to the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is less than or equal to the operand and false otherwise. (Bool)

Params
Value: Any

# IsAudio
Checks the input value if it is an Audio file. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Audio, and false otherwise. (Bool)

# ToInt4
Converts various input types to a vector of four Int elements. If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of four Int elements. (Any)

# IsMoreEqual
Checks if the input is greater than or equal to the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is greater than or equal to the operand and false otherwise. (Bool)

Params
Value: Any

# Remove
Removes all elements from a sequence that match the given condition. Can also take these matched indices and remove corresponding elements from a joined sequence.

In: Any input is ignored. (None)
Out: Output is the filtered sequence. ([Any])

Params
From: Var([Any])
Join: Var([Any])/[Var([Any])]
Predicate: Shard/[Shard]
Unordered: Bool

# Pause
Pauses the wire for a given amount of time.

In: Input is ignored. (Any)
Out: Passes the input value through. (Any)

Params
Time: None/Float/Int/Var(Float)/Var(Int)

# Merge
Combine two tables into one, with the input table taking priority over the operand table, which will be written and returned as output. This shard is useful in scenarios where you need to merge data from different sources while keeping the priority of certain values.

In: ({Any})
Out: ({Any})

Params
Target: Var({Any})

# IsNot
Checks if the input is not equal to the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is not equal to the operand and false otherwise. (Bool)

Params
Value: Any

# IsNone
Gets whether the type of the input is `None`.

In: The value which type to check against. (Any)
Out: `true` is the type of input is `None`; otherwise, `false`. (Bool)

# IsAlmost
Checks whether the input is almost equal to a given value.

In: The input can be of any number type or a sequence of such types. (Float/Float2/Float3/Float4/Int/Int2/Int3/Int4/Int8/Int16/[Any])
Out: true if the input is almost equal to the given value; otherwise, false. (Bool)

Params
Value: Float/Float2/Float3/Float4/Int/Int2/Int3/Int4/Int8/Int16/[Any]
Threshold: Float/Int

# Return
Stops the current flow and outputs the provided input. This shard is used to exit the execution of the current wire early within loops or conditional flows, returning the specified input.

In: The input to return (when supported) and stop the flow. (Any)
Out: This shard does not produce an output as it stops the flow. (None)

# Math.Subtract
This shard subtracts the value provided in the Operand parameter from the input value.

In: The value or the sequence of values to subtract the value specified in the Operand parameter from. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the subtraction. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Sort
Sorts the elements of a sequence. Can also move around the elements of a joined sequence in alignment with the sorted sequence.

In: Any input is ignored. (None)
Out: Output is the sorted sequence. ([Any])

Params
From: Var([Any])
Join: Var([Any])/[Var([Any])]
Desc: Bool
Key: Shard/[Shard]/None

# Update
Modifies the value of an existing mutable variable.

In: The value to be set to the variable. (Any)
Out: The input value is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# Repeat
Repeat an action a given number of times or until the 'Until' parameter returns true.

In: The input will be passed to both the action and the `Until` condition if used. (Any)
Out: The output of this shard will be its input. (Any)

Params
Action: Shard/[Shard]
Times: Int/Var(Int)/None
Forever: Bool
Until: Shard/[Shard]/None

# Limit
This shard truncates the input sequence to the specified number of elements(specified by the Max parameter) and outputs the truncated sequence.

In: The input sequence to truncate. ([Any])
Out: The truncated sequence (or a single element if Max is 1). (Any)

Params
Max: Int

# Take
Extracts one or more elements from a sequence or values from a table using the provided indices or keys. This operation is non-destructive and does not modify the target sequence or table. If the key cannot be established to exist at compose time, the output will be of type Any.

In: The sequence or table from which elements or values will be extracted. (Int2/Int3/Int4/Int8/Int16/Float2/Float3/Float4/Bytes/Color/String/[Any]/{Any})
Out: The extracted elements from a sequence or values from a table. If the key cannot be established to exist at compose time, the output will be of type Any. (Any)

Params
Indices/Keys: Any/Var(Any)

# Count
This shard counts the sequence, string or table variable specified in the Name parameter. If the variable specified is a string, it will count the number of characters. If the variable specified is a sequence, it will count the number of elements. If the variable specified is a table, it will count the number of key-value pairs.

In: The input of this shard is ignored. (None)
Out: Outputs the count of characters, elements, or key-value pairs in the specified variable. If the variable type does not match, it outputs 0. (Int)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# DropFront
Drops the first element of the sequence variable. Works only on sequences. If the variable is not a sequence, it simply passes through without failing.

In: Any input is ignored. (Any)
Out: The input is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# LogType
Logs the type of the value to the console along with an optional prefix string. The logging level can be specified to control the verbosity of the log output.

In: The value whose type will be logged to the console. (Any)
Out: The same value that was inputted, unmodified. (Any)

Params
Prefix: String
Level: LogLevel/Var(LogLevel)
Name: String/Var(String)

# ForEach
Processes every element or key-value pair of a sequence/table with the shards specified in the `Apply` parameter. Note that this shard is able to use the $0 and $1 internal variables, as well as $i for the current index.

In: Sequence/table whose elements or key-value pairs need to be processed. ([Any]/{Any})
Out: Outputs the input value, passed through unchanged. ([Any]/{Any})

Params
Apply: Shard/[Shard]

# Math.RadiansToDegrees
This shard converts the input angle from radians to degrees. The conversion is done using the formula: degrees = radians * (180 / π).

In: Takes a float value representing an angle in radians. (Float)
Out: Outputs a float value representing the input angle in degrees. (Float)

# ExpectInt4Seq
Checks if the input value is a sequence of Int4 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Int4 vectors. ([Int4])

Params
Unsafe: Bool

# Drop
Drops the last element of the sequence variable. Works only on sequences. If the variable is not a sequence, it simply passes through without failing.

In: Any input is ignored. (Any)
Out: The input is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# PopFront
Pops (removes and outputs) the first element of the sequence variable. Works only on sequences. If the variable is not a sequence or the sequence is empty, an error is thrown.

In: Any input is ignored. (None)
Out: The element popped from the sequence. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# Is
Checks if the input is equal to the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is equal to the operand and false otherwise. (Bool)

Params
Value: Any

# IsLess
Checks if the input is less than the operand.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input is less than the operand and false otherwise. (Bool)

Params
Value: Any

# Sequence
Creates an empty sequence (or sequence in a table if a key is passed). Useful to declare and specify types.

In: Any input is ignored. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Clear: Bool
Type: None/Type

# Ref
Creates an immutable reference variable. Once created this variable cannot be changed.

In: The value to be set to the variable. (Any)
Out: The input value is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Overwrite: Bool

# Shards.EnumTypes
Returns a sequence of all enum types in the system.

In: (None)
Out: ([Int])

# Const
Declares an un-named constant value (of any data type).

In: Any input is ignored. (None)
Out: The declared constant value. (Any)

Params
Value: Any

# Cos (Math.Cos)
This shard calculates the cosine of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the cosine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Decompose
This shard converts a 4x4 transformation matrix (a sequence of four float 4 vectors) into a table containing its constituent Translation, Rotation, and Scale components. The table has a Translation key with a float3 vector value representing positions on the x, y, z axes, a Rotation key with a float4 vector value representing the quaternion rotation, and a Scale key with a float3 vector value, representing the size on the x, y, z axes. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} A float3 vector is a vector with 3 float elements while a float4 vector is a vector with 4 float elements. 

In: Takes a 4x4 transformation matrix as input. This matrix should be a sequence of four float4 vectors representing the combined translation, rotation, and scale transformations. ([Float4](4))
Out: Outputs a table containing the Translation, Rotation, and Scale components. Eg. {translation: @f3(1 2 3), rotation: @f4(0 0 0 1), scale: @f3(1 1 1)} ({translation: Float3 rotation: Float4 scale: Float3})

# IsAnyLess
Checks if any element in the input is less than the given value. It outputs true if any element is less and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if any element in the input is less than the specified value and false otherwise. (Bool)

Params
Value: Any

# Math.Cos
This shard calculates the cosine of the given input, where the input is the angle in radians.

In: The input float or sequence of floats to calculate the cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the cosine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Time.EpochLocal
This shard outputs the amount of time that has elapsed from the Unix epoch to the current local system time in seconds.

In: The input of this shard is ignored. (None)
Out: Amount of time since the Unix epoch in local time seconds. (Int)

# LastError
This shard outputs the last error message that occurred as a string.

In: The input of this shard is ignored. (None)
Out: The last error message that occurred as a string. (String)

# Math.Multiply
This shard multiplies the input value by the value provided in the Operand parameter.

In: The value or the sequence of values to multiply the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the multiplication. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Shards.Help
Returns a table of help information for the shard specified by the input name.

In: (String)
Out: ({Any})

# IsFloat4
Checks the input value if it is a vector with float Float elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Float4, and false otherwise. (Bool)

# Isolate
Isolates the inner shards' environment by only allowing certain variables

In: (Any)
Out: (Any)

Params
Contents: Shard/[Shard]
Include: None/[String]
Exclude: None/[String]

# First
Returns the first element from a sorted table or sequence. For tables, returns a [key, value] pair. Returns None if empty. Note: This operation is fast but unsafe unless the output is cloned (using Set instead of Ref) when combined with await or suspended wire flow.

In: A table or sequence to get the first element from. ([Any]/{Any})
Out: For sequences: the first value. For tables: a [key, value] pair. Returns None if input is empty. (Any)

# ToInt
Converts various input types to type Int.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a numerical whole number without any fractional or decimal component. (Any)

# ExpectTable
Checks the input value if it is a Table. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Table. ({})

# Math.Mean
Calculates the average value of a sequence of floating point numbers.

In: The sequence of floating point numbers to calculate the average of. ([Float])
Out: The calculated average as a float. (Float)

Params
Kind: Mean

# ToFloat
Converts various input types to type Float.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a numerical value that can include a fractional or decimal component. (Any)

# ToFloat4
Converts various input types to a vector of Four Float elements. If a single value or a collection with less than 4 elements is provided, the remaining unaccounted elements in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of Four Float elements. (Any)

# ForRange
Executes a series of shards while an iteration value is within a specified range (inclusive). Action input is the current index, not the input.

In: The input value is not used and will pass through unchanged. (Any)
Out: The output of this shard will be its input. (Any)

Params
From: Int/Var(Int)
To: Int/Var(Int)
Action: Shard/[Shard]/None

# IsAllLess
Checks if all elements in the input are less than the given value. It outputs true if all elements are less and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are less than the specified value and false otherwise. (Bool)

Params
Value: Any

# Assoc
Updates a sequence (array) or a table (associative array/ dictionary) on the basis of an input sequence.

In: Input sequence that defines which element in the target sequence or table needs to be updated and with what value. Should have even number of elements. ([Any])
Out: Modified array or table. Has the same type as the array or table on which Assoc was applied. ([Any])

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# Math.Add
This shard adds the input value to the value provided in the Operand parameter.

In: The value or the sequence of values to add the value specified in the Operand parameter to. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the addition. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# PrependTo
Prepends the input to the context variable passed to `Collection`.

In: The value to prepend to the collection. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Collection: Var([Any])/Var(String)/Var(Bytes)

# TraitId
Retrieves the hash id of the given trait

In: (None)
Out: (Int2)

Params
Trait: Trait

# ExpectFloat2Seq
Checks if the input value is a sequence of Float2 vectors. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Float2 vectors. ([Float2])

Params
Unsafe: Bool

# IsFalse
Gets whether the input is `false`.

In: The value to check against. (Bool)
Out: `true` if the input is `false`; otherwise, `false`. (Bool)

# String.Join
This shard concatenates all the elements of a string sequence, using the specified separator between each element.

In: A sequence of string values that will be joined together. ([String Bytes])
Out: A string consisting of all the elements of the sequence separated by the specified separator. (String)

Params
Separator: String

# Push
Pushes a new value into a sequence variable. If the variable does not exist, it will be created.

In: The value to push into the sequence. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Clear: Bool

# StringToBytes
Converts a string to its byte representation.

In: A string to be converted to bytes. (String)
Out: The byte representation of the input string. (Bytes)

# GlobalOnce
Executes the shard or sequence of shards only once per mesh global execution.

In: (Any)
Out: (Any)

Params
Action: Shard/[Shard]

# IsValidNumber
Checks if the input is a valid floating-point number (not zero, subnormal, infinity, or NaN). Outputs true if the input is a normal floating-point number, otherwise outputs false.

In: The floating-point number to be checked. (Float)
Out: Outputs true if the input is a valid normal floating-point number, otherwise outputs false. (Bool)

# Math.Atan
This shard calculates the inverse tangent of the given input, where the input is the tangent value. The output is the angle in radians whose tangent is the input value.

In: The input float or sequence of floats to calculate the inverse tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose tangent is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Div (Math.Divide)
This shard divides the input value by the value provided in the Operand parameter.

In: The value or the sequence of values to divide the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: This shard outputs the result of the division. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.Log
This shard calculates the natural logarithm for the given input. The output is the exponent to which e must be raised to obtain the input value.

In: The input float or sequence of floats to calculate the natural logarithm of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the natural logarithm of the input. (Float/Float2/Float3/Float4/Color/[Any])

# IsImage
Checks the input value if it is an Image. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Image, and false otherwise. (Bool)

# Atan (Math.Atan)
This shard calculates the inverse tangent of the given input, where the input is the tangent value. The output is the angle in radians whose tangent is the input value.

In: The input float or sequence of floats to calculate the inverse tangent of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose tangent is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Math.TGamma
This shard calculates the gamma function of the given input. The gamma function is a mathematical function that extends the concept of factorial to non-integer and complex numbers.

In: This shard calculates the gamma function of the given input. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the gamma function of the input. The output is always positive for positive inputs. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Orthographic
This shard creates a 4x4 orthographic projection matrix based on the width size, height size, near, and far planes specified in the appropriate parameters. A 4x4 matrix is a sequence with exactly 4 float4 vectors while a float4 vector is a vector with 4 float elements.

In: The input of this shard is ignored. (None)
Out: Outputs a 4x4 orthographic projection matrix (a sequence of four float4 vectors). ([Float4](4))

Params
Width: Int/Float
Height: Int/Float
Near: Int/Float
Far: Int/Float

# IsColor
Checks the input value if it is vector of four color channels (RGBA). The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Color, and false otherwise. (Bool)

# SwitchTo
Suspends the current Wire and switches execution to the specified Wire.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: The output of this shard is the output of the Wire that execution was switched to, upon switching back to the parent Wire. (Any)

Params
Wire: Wire/String/None
Restart: Bool

# Bytes.Join
This shard will concatenate a sequence of strings or bytes into a single string or byte array and output it as a byte array.

In: The sequence of strings or byte array to concatenate. ([String Bytes])
Out: The concatenated string or bytes represented as a byte array. (Bytes)

# ExpectSeq
Checks if the input value is a sequence; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is a sequence. ([Any])

# ExpectBool
Checks the input value if it is a Boolean. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Bool. (Bool)

# Profile
This shard outputs the amount of time it took to execute the shards provided in the Action parameter, automatically choosing the most appropriate time unit (ns, μs, ms, s).

In: The input of this shard will be provided as input to the shards in the Action parameter. (Any)
Out: The output of this shard will be the output of the shards in the Action parameter. (Any)

Params
Action: Shard/[Shard]
Label: String

# Math.Erf
This shard calculates the error function of the given input. The error function is related to the probability that a random variable with normal distribution of mean 0 and variance 1/2 falls in the range specified by the input value.

In: The input float or sequence of floats to calculate the error function of. This can be any real number. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs probability result of the error function of the input. The output is always between -1 and 1. (Float/Float2/Float3/Float4/Color/[Any])

# Shards.Enumerate
Returns a sequence of all shard names in the system.

In: (None)
Out: ([String])

Params
Category: None/String

# BytesToString
Converts a sequence of bytes into a string. Each byte in the sequence is interpreted as a character in the resulting string.

In: Accepts a byte array as input. Each byte in the sequence is interpreted as a character. (Bytes)
Out: The output is a string created from the input sequence of bytes. (String)

# IsString
Checks the input value if it is a String. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type String, and false otherwise. (Bool)

# ExpectAudio
Checks the input value if it is an Audio file. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Audio. (Audio)

# Shuffle
Shuffles the elements of the sequence variable. Works only on sequences. If the variable is not a sequence, it simply passes through without failing.

In: Any input is ignored. (Any)
Out: The input is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool

# Math.AxisAngleX
This shard creates a rotation quaternion for rotation around the X-axis. It takes a float input representing the angle in radians and outputs the rotation quaternion as a float4 vector. A float4 vector is a vector with 4 float elements.

In: Takes a float value representing the X rotation in radians. (Float)
Out: Outputs a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the X-axis. (Float4)

# IsNotNone
Gets whether the type of the input is different from `None`.

In: The value which type to check against. (Any)
Out: `true` is the type of input different from `None`; otherwise, `false`. (Bool)

# Restart
Restarts the current flow with the provided input. This shard is used to restart the execution of the current wire from the beginning, using the same input. It ensures that the input type matches the wire's root input type. Note: This is a flow stopper and will not continue to subsequent shards in the current execution sequence.

In: The input to restart the wire with. Must match the wire's root input type. (Any)
Out: This shard does not produce an output as it restarts the flow. (None)

# Clamp
This shard ensures the input value falls within the specified range. If the value falls below the minimum, the Min value is returned. If the value exceeds the maximum, the Max value is returned. Otherwise, the value is returned unchanged.

In: The value to clamp. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color)
Out: The clamped value. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color)

Params
Min: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])
Max: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# Math.Cosh
This shard calculates the hyperbolic cosine of the given input, where the input is the real number. The hyperbolic cosine is a hyperbolic function that is analogous to the circular cosine function, but it uses exponential functions instead of angles.

In: The input float or sequence of floats to calculate the hyperbolic cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the hyperbolic cosine of the input. (Float/Float2/Float3/Float4/Color/[Any])

# ExpectInt8
Checks the input value if it is a vector with eight Int elements. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Int8. (Int8)

# Math.Sqrt
This shard calculates the square root of the given input.

In: The input float or sequence of floats to calculate the square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# PauseMs
Pauses the wire for a given amount of time.

In: Input is ignored. (Any)
Out: Passes the input value through. (Any)

Params
Time: None/Int/Var(Int)

# Branch
Creates a branch from the specified Behavior and schedules all the Wires specified. Every time this shard is called, it will progress the state of all the Wires specified asynchronously and continue execution of the current Wire. This shard is like a mass Step, where it Steps all the Wires specified.

In: Any input type is accepted. The input value will pass through unchanged. (Any)
Out: Outputs the input value, passed through unchanged. (Any)

Params
Wires: Wire/[Wire]/None
FailureBehavior: BranchFailure
CaptureAll: Bool
Mesh: None/Mesh

# Math.LookAt
This shard creates a 4x4 view matrix (a sequence of four float4 vectors) for a camera based on the camera's position and a target point which is represented as a table with two float3 vectors: 'Position' and 'Target', that is passed as input. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

In: Takes a table with two float3 values: 'Position' (the camera's position) and 'Target' (the point the camera is looking at). Eg. { Position: @f3(1 2 3) Target: @f3(4 5 6) } ({Position: Float3 Target: Float3})
Out: Outputs a 4x4 view matrix (a sequence of four float4 vectors). ([Float4](4))

# ToFloat2
Converts various input types to a vector of two Float elements. If a single value or a collection with only one element is provided, the second element in the resulting vector will be set to 0.

In: Takes input values of type `Int`, `Float`, `String`, or a collection  of `Int`s and `Float`s. Note that the shard can only convert strings that represent numerical values, such as "5", and not words like "Five". (Any)
Out: Outputs a vector of two Float elements. (Any)

# Table
Creates an empty table. Useful to declare and specify types.

In: Any input is ignored. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Type: None/Type

# CaptureLog
Captures log messages based on specified parameters, such as the number of messages to retain, the minimum log level, and the log format pattern. It can optionally suspend execution until new log messages are available.

In: The input is ignored. This shard captures log messages based on specified parameters. (None)
Out: A sequence of captured log messages. ([String])

Params
Size: Int
MinLevel: String
Pattern: String
Suspend: Bool

# IsInt16
Checks the input value if it is a vector of 16 Int elements. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int16, and false otherwise. (Bool)

# Math.Lerp
Linearly interpolate between the start value specified in the `First` parameter and the end value specified in the `Second` parameter based on the factor provided as input.

In: The factor to interpolate between the start and end values. (Float)
Out: The interpolated value between the start and end values based on the factor provided as input. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
First: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)
Second: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)

# Time.EpochMs
This shard outputs the the amount of time that has elapsed from the Unix epoch to the current system time in milliseconds.

In: The input of this shard is ignored. (None)
Out: Amount of time since the Unix epoch in milliseconds. (Int)

# Math.Or
This shard performs a bitwise OR operation on the input value with the value specified in the Operand parameter and outputs the result. A bitwise OR operation is a binary operation that compares each bit of the binary representations of two numbers and outputs 1 if either bit is 1 and 0 if both bits are 0. The shard then outputs a value, whose binary representation is the concatenation of the resulting 1s and 0s from the Or comparison.

In: The value or the sequence of values to compare the value specified in the Operand parameter with. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)
Out: This shard outputs the value resulting from the OR operation. (Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool)

Params
Operand: Int/Int2/Int3/Int4/Int8/Int16/Color/[Any]/Bool/Var(Bool)

# Math.Log2
This shard calculates the base 2 logarithm for the given input. The output is the exponent to which 2 must be raised to obtain the input value.

In: The input float or sequence of floats to calculate the base 2 logarithm of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the base 2 logarithm. (Float/Float2/Float3/Float4/Color/[Any])

# Cond
Takes a sequence of conditions and predicates. Evaluates each condition one by one and if one matches, executes the associated action.

In: The value that will be passed to each predicate and action to execute. (Any)
Out: The input of the shard if `Passthrough` is `true`; otherwise, the output of the action of the first matching condition. (Any)

Params
Wires: [Shard [Shard] None]
Passthrough: Bool
Threading: Bool

# Math.Negate
This shard reverses the sign of the input. (A positive number becomes negative, and vice versa).

In: The float or integer (or sequence of floats or integers) to reverse the sign of. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: The input with its sign reversed. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

# Expect
Checks if the input value matches the expected type specified by the 'Type' parameter. The shard outputs the input value unchanged if it is of the appropriate type, the shard will trigger an error, preventing further execution. The 'Unsafe' parameter can be set to skip deep type hashing and comparison to improve performance.

In: Input of any type is accepted. (Any)
Out: The input value unchanged if it matches the expected type. (Any)

Params
Type: Type
Unsafe: Bool

# SetLogLevel
This shard changes the log level to the level specified by the string passed as input. 

In: A string representing the new log level (e.g., 'debug', 'info', 'warn', 'error', 'critical'). (String)
Out: Outputs the input value, passed through unchanged. (Any)

# Slice
Extracts characters from a string or elements from a sequence based on the start and end positions/indices and an increment parameter. Operation is non-destructive; the target string/sequence is not modified.

In: The string or sequence from which characters/elements have to be extracted. ([Any]/Bytes/String)
Out: The extracted characters/elements. (Any)

Params
From: Int/[Int]/Var(Int)/Var([Int])
To: Int/[Int]/Var(Int)/Var([Int])/None
Step: Int

# Math.Cross
This shard computes the cross product of the float3 vector (or sequence of float3 vectors) provided as input and the float3 vector provided in the Operand parameter and outputs the result as a float3 vector (or sequence of float3 vectors). A float3 vector is a vector with 3 float elements.

In: Accepts float3 vector (a vector with 3 float elements) as input. (Float3/[Float3])
Out: Outputs the result of the cross product as a float3 vector or a sequence of float3 vectors if the input was a sequence of float3 vectors. (Float3/[Float3])

Params
Operand: Float2/[Float2]/Float3/[Float3]/Float4/[Float4]/Var(Float2)/Var([Float2])/Var(Float3)/Var([Float3])/Var(Float4)/Var([Float4])

# Highest
Takes a sequence and outputs the element with the highest value.

In: A sequence of elements of any type. ([Any])
Out: Outputs the element with the highest value. (Any)

# Math.Log10
This shard calculates the base 10 logarithm for the given input. The output is the exponent to which 10 must be raised to obtain the input value.

In: The input float or sequence of floats to calculate the base 10 logarithm of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the base 10 logarithm of the input. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Erfc
This shard calculates the complementary error function of the given input. The complementary error function is related to the probability that the absolute value of a random variable with normal distribution of mean 0 and variance 1/2 is greater than the input value.

In: The input float or sequence of floats to calculate the complementary error function of. This can be any real number. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the probability result of the complementary error function of the input. The output is always between 0 and 2. (Float/Float2/Float3/Float4/Color/[Any])

# Math.Slerp
This shard performs Spherical Linear Interpolation (SLERP) between two quaternions (represented as float4 vectors). It smoothly interpolates between the quaternions specified in the 'First' parameter and 'Second' parameter based on the input interpolation factor. A float4 vector is a vector with 4 float elements.

In: Takes a float value between 0 and 1 representing the interpolation factor. (Float)
Out: Outputs a float4 vector representing the interpolated quaternion. (Float4)

Params
First: Float4/Var(Float4)
Second: Float4/Var(Float4)

# IsAllNot
Checks if all elements in the input are not equal to the given value. It outputs true if all elements are not equal and false otherwise.

In: Input of any type is accepted. For types without inherent value (e.g., None, Bool), a lexicographical comparison is used. (Any)
Out: Outputs true if all elements in the input are not equal to the specified value and false otherwise. (Bool)

Params
Value: Any

# Set
Creates a mutable variable and assigns a value to it.

In: The value to be set to the variable. (Any)
Out: The input value is passed through as the output. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Tracked: Bool

# Math.Translation
This shard creates a 4x4 translation matrix (a sequence of four float4 vectors) from a float3 vector input representing the translation in x, y, and z directions. A float4 vector is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.

In: Takes a float3 vector (a vector with 3 float elements) that represents the translation in x, y, and z directions. The first element in the vector being x, the second y and the third z. (Float3)
Out: Outputs a 4x4 translation matrix (a sequence of four float4 vectors). ([Float4](4))

# Math.Acosh
This shard calculates the inverse hyperbolic cosine of the given input, where the input is the hyperbolic cosine value. The output is the real number whose hyperbolic cosine is the input value.

In: The input float or sequence of floats to calculate the inverse hyperbolic cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the real number whose hyperbolic cosine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Match
Compares the input with the declared cases EXACT values (use Cond for more complex matching logic) in order of the declaration and activates the shard of the first matched case.

In: The value that's compared with the declared cases. (Any)
Out: Same value as input if `:Passthrough` is `true` else the output of the matched case's shard if `:Passthrough` is `false`. (Any)

Params
Cases: [Any]
Passthrough: Bool

# Get
Reads the value of the specified variable.

In: Any input is ignored. (None)
Out: The output is the value read from the specified variable. (Any)

Params
Name: String/Var(Any)
Key: Any
Global: Bool
Default: Any

# String.Ends
This shard checks if the input string ends with the string specified in the With parameter. If the input string does contain the string specified, the shard will output true. Otherwise, it will output false.

In: The string to check. (String)
Out: True if the input string ends with the string specified, false otherwise. (Bool)

Params
With: String/Var(String)

# ExpectBoolSeq
Checks if the input value is a sequence of Bools. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Booleans. ([Bool])

Params
Unsafe: Bool

# Recur
The Recur shard executes the Wire that calls it recursively, using the output of the Wire as input again, until the base cases are reached. It then combines the results to produce the final result. For the shard not to Recur endlessly, a base case needs to be defined, usually through a When or If shard.

In: After the first cycle of Recur, the output of the Wire that calls Recur will be fed back as input for the next cycle. (Any)
Out: The output of Recur will be the output of the Wire that calls it. (Any)

# Min
This shard compares the input with the value specified in the `Operand` parameter and outputs the smaller value.

In: The first value to compare with. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])
Out: The smaller value between the input and the operand. (Int/Int2/Int3/Int4/Int8/Int16/Float/Float2/Float3/Float4/Color/[Any])

Params
Operand: Int/Var(Int)/Int2/Var(Int2)/Int3/Var(Int3)/Int4/Var(Int4)/Int8/Var(Int8)/Int16/Var(Int16)/Float/Var(Float)/Float2/Var(Float2)/Float3/Var(Float3)/Float4/Var(Float4)/Color/Var(Color)/[Any]/Var([Any])

# IsWire
Checks the input value if it is a Wire. The shard will return true if the input is of the appropriate type, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Wire, and false otherwise. (Bool)

# ExpectFloatSeq
Checks if the input value is a sequence of Floats. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Floats. ([Float])

Params
Unsafe: Bool

# FastSqrt (Math.FastSqrt)
This shard calculates the square root of the given input.

In: The input float or sequence of floats to calculate the square root of. This value must be a positive number or sequence of positive numbers. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the square root of the input. (Float/Float2/Float3/Float4/Color/[Any])

# IsInt
Checks the input value if it is of type Int. The shard will return true if the input value is of type Int, and false otherwise.

In: Input of any type is accepted. (Any)
Out: Outputs true if the input value is of type Int, and false otherwise. (Bool)

# Or
Computes the logical OR between the input of this shard and the output of the next shard. If the input is true, the flow stops and succeeds; if false, the flow continues with the next shard. Typically used within conditional flows (e.g., If, When) to chain conditions. Note: Outside a conditional flow, it might restart the current wire, which can be used as a trick in certain scenarios.

In: If true, the flow stops and succeeds; otherwise, the flow continues. (Bool)
Out: The output of this shard will be the input of the current conditional flow or wire. (Bool)

# Acos (Math.Acos)
This shard calculates the inverse cosine of the given input, where the input is the cosine value. The output is the angle in radians whose cosine is the input value.

In: The input float or sequence of floats to calculate the inverse cosine of. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the angle in radians whose cosine is the input value. (Float/Float2/Float3/Float4/Color/[Any])

# Do
Schedules and executes the specified Wire inline of the current Wire. The specified Wire needs to complete its execution before the current Wire continues its execution. This means that a pause in execution of the child Wire will also pause the parent Wire.

In: Any input type is accepted. The input of this shard will be given as input for the specified Wire (Any)
Out: The output of this shard will be the output of the Wire that is executed. (Any)

Params
Wire: Wire/String/None

# Erase
Deletes an index or indices from a sequence or a key or keys from a table.

In: Any input is ignored. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Indices: Any/Var(Any)
Name: String/Var(Any)
Key: Any
Global: Bool

# AppendTo
Appends the input to the context variable passed to `:Collection`.

In: The value to append to the collection. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
Collection: Var([Any])/Var(String)/Var(Bytes)

# Time.DeltaMs
Outputs the time between the last call of this shard and the current call in milliseconds, capped to a limit

In: The input of this shard is ignored. (None)
Out: Outputs the amount of time that has elapsed in milliseconds. (Float)

# Peek
Checks if another Wire has ended (Note that a looped Wire will never end). Outputs the Wire's output if it has ended, or none if it is still in progress.

In: The input of this shard is ignored. (None)
Out: This shard either outputs none if the peeked Wire is still in progress, or the peeked Wire's output if it has ended. (Any)

Params
Wire: Wire/String/None/Var(Wire)

# Math.QuatMultiply
This shard multiplies two quaternions (represented as float4 vectors) together. It combines the two rotations by multiplying the input quaternion with the operand quaternion. A float4 vector is a vector with 4 float elements.

In: Takes a float4 vector representing the quaternion to be multiplied. (Float4)
Out: Outputs a float4 vector representing the resulting quaternion after multiplication. (Float4)

Params
Operand: Float4/Var(Float4)

# Swap
Swaps the values of the two variables passed to it via `First` and `Second` parameters.

In: Input is ignored. (Any)
Out: The input to this shard is passed through as its output. (Any)

Params
First: Var(Any)
Second: Var(Any)

# Reverse
This shard reverses the order of the elements in the input sequence or string.

In: The input sequence or string to be reversed. ([Any]/String/Bytes)
Out: Outputs the reversed sequence or string. ([Any]/String/Bytes)

# ExpectBytes
Checks the input value if it is of type Bytes. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: Input of any type is accepted. (Any)
Out: Outputs the input value unchanged if it is of type Bytes. (Bytes)

# Math.Expm1
This shard calculates the exponential function with base e (Euler's number) for the given input and subtracts 1 from the result.

In: The input float or sequence of floats used as the exponent for the base e exponential function. (Float/Float2/Float3/Float4/Color/[Any])
Out: Outputs the result of the exponential operation. (Float/Float2/Float3/Float4/Color/[Any])

# ToBytes
This shard takes a value and converts it to a serialized binary representation (a serialized byte array).

In: This shard will take any value. (Any)
Out: This shard will return a seriliazed byte array representing the input value. (Bytes)

# ToAny
Converts an integer, bytes, or string value into its hexadecimal string representation.

In: Converts the input to any type (Any)
Out: The same value as the input but typed as Any. (Any)

# Time.Now
This shard outputs the amount of time that has elapsed since the shards application or script was launched in seconds.

In: The input of this shard is ignored. (None)
Out: Outputs the amount of time that has elapsed in seconds. (Float)

# ParseInt
Converts the string representation of a number to its signed integer equivalent.

In: A number represented as a string. (String)
Out: A signed integer equivalent to the number contained in the string input. (Int)

Params
Base: Int

# String.ToUpper
This shard converts all characters in the input string to uppercase.

In: The string to convert to uppercase. (String)
Out: The input string converted to uppercase. (String)

# Time.ToString
This shard converts time into a human readable string.

In: The time to convert. (Int/Float)
Out: A string representation of the time. (String)

Params
Millis: Bool

# ExpectStringSeq
Checks if the input value is a sequence of Strings. The shard outputs the input value unchanged if it is of the appropriate type; otherwise, the shard will trigger an error, preventing further execution.

In: This shard accepts a sequence of values. (Any)
Out: Outputs the input value unchanged if it is a sequence of Strings. ([String])

Params
Unsafe: Bool

# RandomBytes
This shard generates a random sequence of bytes. The size of the sequence is specified in the Size parameter.

In: The input of this shard is ignored. (None)
Out: Outputs a random sequence of bytes. (Bytes)

Params
Size: Int

# RandomFloat
This shard generates a random float between 0 and the maximum value specified in the Max parameter (exclusive).

In: The input of this shard is ignored. (None)
Out: Outputs a random float. (Float)

Params
Max: None/Float/Var(Float)

# RandomInt
This shard generates a random integer between 0 (inclusive) and the maximum value specified in the Max parameter (exclusive).

In: The input of this shard is ignored. (None)
Out: Outputs a random integer. (Int)

Params
Max: None/Int/Var(Int)

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

