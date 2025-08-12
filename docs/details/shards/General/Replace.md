The sequence provided in the `Replacements`parameter can only be a sequence containing a single element or a sequence of the same length as the `Patterns` parameter.

If the `Replacements` sequence has more than 1 element, each element in the `Patterns` sequence is a pattern to be matched against and will be replaced with the corresponding element in the `Replacements` sequence. Eg. `"Hello World" | Replace(Patterns: ["Hello", "World"], Replacements: ["Hi", "Universe"])` will replace `"Hello"` with `"Hi"` and World with Universe, thus returning `"Hi Universe"`.

If the `Replacements` sequence has only 1 element, that element will be used to replace all occurrences of the patterns in the `Patterns` sequence. Eg. `"Hello World" | Replace(Patterns: ["Hello", "World"], Replacements: ["Hi"])` will replace `"Hello"` with `"Hi"` and `"World"` with `"Hi"`, thus returning `"Hi Hi"`.



