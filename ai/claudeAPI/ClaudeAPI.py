# ClaudeAPI

# from claudetools.tools.tool import Tool
# from pydantic import BaseModel, Field
from typing import List, Dict
import json
import requests
import pandas as pd
import os

from config import ANTHROPIC_API_KEY
import anthropic

ANTHROPIC_API_KEY = os.getenv('ANTHROPIC_API_KEY')

prompt = """
    spawn 6 trees in a grid
"""

DEPENDENCY_PROMPT = """
I need your help to understand and generate some commands in the form of Shards scripts to be sent to our game creation system (Formabble), which is Right Handed with Y as up axis, from whatever the user inputs.

### A bit about Shards:

String: "hello world"
Integer: 1
Float: 1.0
Float2: @f2(1.0 2.0)
Float3: @f3(1.1 2.0 3.0)
We also have @f4
Int vectors also: @i2, @i3, @i4, @i8 and @i16@ is a prefix for our built-ins, definitions and templates to distinguish between variables
; is the line comment symbol, ignore comments

Shards ["Hello " @name] | String.Join | Log >= message [] is a sequence, in the above you can see that @name is something defined, while message is a variable we assign with >= which is a mutable assignment operator, as you can see in Shards everything flows from left to right from top to bottom, even assignments.
> is used to update mutable values
= is used to assign am immutable reference

Shards Count(seq) When({IsMore(0) | And | IsMore(selected-idx)} {   ; DO SOMETHING With Count(seq) output }) 
In this Shards script, we start by counting the elements in seq. Then, we evaluate two conditions in sequence: first, we check if the count is greater than zero.
If this is true, we then check if the count is also greater than selected-idx. Both conditions must be satisfied for the script to execute the code within the block

Types are strict, so when you use or pass a constant 0 will be an integer 0.0 will be float
Same when declaring variables:
0.0 >= float-var0 >= int-var
Types are also strict when doing math! so please use right types, you can use ToFloat, ToInt etc to convert.
Variables must be unique! Don't Set with same name. Unless it's an Update (>)
Example, idx | Math.Mod(10), if you need float output use idx | Math.Mod(10) | ToFloat
You often mistake even the opposite, if for some reason idx is a float you also need to do this: idx | ToInt | Math.Mod(10) | ToFloat
E.g.
idx | ToFloat | Math.Divide(5.0) | Math.Floor = row row | Math.Mod(2) | ToFloat | Math.Multiply(1.0) = zExtraOffset ; <------- WRONG, should be: row | ToInt| Math.Mod(2) | ToFloat | Math.Multiply(1.0) = zExtraOffset
Or
idx | Math.Mod(5) = column column | ToFloat | Math.Multiply(1.75) = zOffset ; column came from Mod output, so was a Int!

Math is done like this:
time | Math.Multiply(0.2) | Math.AxisAngleX | Math.Rotation ; Rotation X | Math.MatMul((time | Math.Multiply(0.7) | Math.AxisAngleY | Math.Rotation)) ; Rotation Y | Math.MatMul((time | Math.Multiply(0.9) | Math.AxisAngleZ | Math.Rotation)) ; Rotation Z | Log+ is Math.Add, - is Math.Subtract, * is Math.Multiply, / is Math.Divide
We don't use regular operators such as (+,-,*,/) in shards and math is always from left to right we don't have math expressions like C,
so this is wrong @f3(1 + index1 2 1 + index2), but it can be achieved using () construct which will wrap the operation and create a temp variable such as:
@f3((1 | Math.Add(x)) 2 (1 | Math.Add(y)))

Also keep in mind that Math.Floor will output a float! so you might need to ToInt its output!!

Also Shards is sort of strongly typed so if you wanna do something like this:
f3(0.0 (idx | Math.Multiply(1.0)) 0.0)
it should be:
@f3(0.0 (idx | ToFloat | Math.Multiply(1.0)) 0.0)

Loops:
0 >= idx ; we need to manage manually the index Repeat({   Msg("Hello")   Math.Inc(idx) } Times: 3)
or
ForRange(0 2 {   = idx ; we get automatically the current index   Msg("Hello") })

Seq access and creation
1 >> s 2 >> s  s | Take(0) | Assert.Is(1)

Tables access and creation
{a: 1 b: 2} = t ; actually any JSON will do, our tables are like json tables  t | Take("a") | Assert.Is(1)
Always prefer declaring tables in JSON style though! and Immutable.

Random numbers!
RandomFloat(Max: 1.0) | IsLess(1.0) | Assert.Is(true) RandomInt(Max: 100) | IsLess(100) | Assert.Is(true)

Remember:
this is wrong:
@f3(randomX 0.0 y | Math.Multiply(10.0)) = position
it should be:
@f3(randomX 0.0 (y | Math.Multiply(10.0))) = position as you need () for such lambda behavior

Some Formabble specific functionality:
* Formalize
* Description
* Spawn or create an entity (a form in our case) from a form asset in the environment or scene (called domain in our case).
* Example
* {Graphics: {"fbl/pose": {translation: @f3(0.0 0.0 0.0) rotation: @f4(0.0 0.0 0.0 1.0) scale: @f3(1.0 1.0 1.0)}}} | Fbl.Formalize("__ASSET_ID__")
* Notes
* Important: Always include all TRS components, even if default!!
* Important: Rotation is always a quaternion
* Dispatch
* Description
* Dispatches a variable change to the requested form in the right subsystem
* Examples
* Math.Decompose = gizmo-trs | gizmo-trs | Take("translation") | Fbl.Dispatch(form-id WirePoolType::Graphics "fbl/pose" Key: "translation") (Key is optional)
* 10.0 | Fbl.Dispatch("cube1" WirePoolType::Graphics "speed")

Now from now on you will wait for user prompts and generate commands.
For now just focus on Formalizing forms. User input will be natural language instructions.
Please output a Shard script with all the code to achieve the requested instructions.
Be brief, omit validation for now.
Oh and please from now on just answer queries related to Formabble and Shards.

Examples:
A RANDOM FOREST
0 >= idx ForRange(0 99 {   idx | ToFloat | Math.Divide(10.0) | Math.Floor = x   idx | Math.Mod(10) = z   RandomFloat(Max: 1.0) | Math.Multiply(10.0) = randomX   RandomFloat(Max: 1.0) | Math.Multiply(10.0) = randomZ   @f3((x | Math.Add(randomX)) 0.0 (z | ToFloat | Math.Add(randomZ))) = position   {Graphics: {"fbl/pose": {translation: position rotation: @f4(0.0 0.0 0.0 1.0) scale: @f3(1.0)}}} | Fbl.Formalize("d-treeDefault")   Math.Inc(idx) })

A GRID OF BLOCKS
0 >= idx ForRange(0 99 {   idx | ToFloat | Math.Divide(10.0) | Math.Floor = x   idx | Math.Mod(10) | ToFloat = z   @f3(x 0.0 z) = position   {Graphics: {"fbl/pose": {translation: position rotation: @f4(0.0 0.0 0.0 1.0) scale: @f3(1.0)}}} | Fbl.Formalize("d-block")   Math.Inc(idx) })

ALWAYS PREFER SIMPLE CODE, DON'T NEED TO OPTIMIZE, UNROLL OPERATIONS ETC!
Your reply should look like this:
MARKDOWN SHARDS CODE BLOCK TO EVAL
LINE BREAK
COMMENTS
"""


full_prompt = DEPENDENCY_PROMPT + "\n" + prompt


client = anthropic.Anthropic(
    # defaults to os.environ.get("ANTHROPIC_API_KEY")
    api_key=ANTHROPIC_API_KEY,
)

message = client.messages.create(
    model="claude-3-opus-20240229",
    max_tokens=1000,
    temperature=0.0,
    system="You are a Shards programming expert.",
    messages=[
        {"role": "user", "content": full_prompt}
    ]
)

# print(message.content[0].text)

# # Trying 3 Claude3 models and 5 prompts

models = ["claude-3-opus-20240229", "claude-3-sonnet-20240229","claude-3-haiku-20240307"]
prompts = [DEPENDENCY_PROMPT + "spawn 6 trees in a grid", 
           "Generate 6 trees spread randomly on a 3x3 grid using d-TreePine, starting from x and z position of 1.0 Keep the y-axis at 0.0",
           "Generate 10 flowers spread randomly on a 2x2 grid using d-FlowerRed, starting from x and z position of 1.0 Keep the y-axis at 0.0 ",
           "Generate a 6x6 hexagonal grid using d-BlockHexagon tiles. Keep the y-axis at -1.0 Offset tile’s x position by 0.5 and z position by 0.865 so they form a cohesive hexagonal grid",
           "Starting from x position 4 and z position 4, generate a 4x4 hexagonal grid using d-BlockSnowHexagon tiles. Keep the y-axis at -1.0 Offset tile’s x position by 0.5 and z position by 0.865 so they form a cohesive hexagonal grid."
           ] # 5 prompts

# Create empty DataFrame
df = pd.DataFrame(columns=['Prompt', 'Model', 'Completion'])

for model in models:
    for prompt in prompts:
        
        message = client.messages.create(
            model=model,
            max_tokens=1000,
            temperature=0.0,
            system="You are a Shards programming expert.",
            messages=[
                {"role": "user", "content": prompt}
            ]
        )
        # Appending the result to the DataFrame
        df = pd.concat([df, pd.DataFrame({'Prompt': prompt, 'Model': model, 'Completion': [message.content[0].text]})], ignore_index=True)

# Saving the results to a CSV file
df.to_csv("claude_model_completions.csv", index=False)

print("Completions saved to model_completions.csv")



