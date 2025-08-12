This shard is only able to parse strings that represent floating-point numbers. It can handle:
- Integer parts eg. `"123"`
- Decimal points & Fractional parts eg. `"123.456"`
- Scientific notation eg. `"1.23e-4"`
- Leading whitespace (which is ignored) eg. `"  123.456"`
- Leading plus or minus signs eg. `"+123.456"` or `"-123.456"`