import { chain, Msg, Assert, Math, Log, Float4, Float3 } from "blocks"
import { SetVariable, Repeat, GetVariable, SwapVariables, SetTableValue, GetTableValue, IPC, AddVariable, GetItems } from "./blocks";

let n = new Node()
n.schedule(chain([
  Msg("Running tests!"),

  "Hello",
  Assert.Is("Hello", true),
  Log(),

  77,
  Assert.Is(77, true),
  Log(),

  10,
  Math.Add(10),
  Assert.Is(10 + 10, true),
  Log(),

  11,
  Math.Subtract(10),
  Assert.Is(11 - 10, true),
  Log(),

  Float4(10.3, 3.6, 2.1, 1.1),
  Math.Multiply(Float4(2, 2, 2, 2)),
  Assert.Is(Float4(10.3 *2, 3.6 * 2, 2.1 *2, 1.1 *2), true),
  Log(),

  Float3(10.3, 2.1, 1.1),
  Math.Multiply(Float3(2, 2, 2)),
  Assert.Is(Float3(10.3 *2, 2.1 *2, 1.1 *2), true),
  Log(),

  10,
  AddVariable("list1"),
  20,
  AddVariable("list1"),
  30,
  AddVariable("list1"),
  GetVariable("list1"),
  Log(),
  GetItems(0),
  Assert.Is(10, true),
  GetVariable("list1"),
  GetItems(1),
  Assert.Is(20, true),
  GetVariable("list1"),
  GetItems(2),
  Assert.Is(30, true),
  Log(),

  0,
  SetVariable("counter"),
  Repeat([
    GetVariable("counter"),
    Math.Add(1),
    SetVariable("counter"),
  ], 5),
  GetVariable("counter"),
  Assert.Is(5, true),
  Log(),

  20,
  SetVariable("a"),
  30,
  SetVariable("b"),
  SwapVariables("a", "b"),
  GetVariable("a"),
  Assert.Is(30),
  GetVariable("b"),
  Assert.Is(20),
  Log(),

  "Value1",
  SetTableValue("tab1", "v1"),
  "Value2",
  SetTableValue("tab1", "v2"),
  GetTableValue("tab1", "v1"),
  Assert.Is("Value1", true),
  Log(),
  GetTableValue("tab1", "v2"),
  Assert.IsNot("Value1", true),
  Log(),

  Msg("All looking good!")
], "general"))

let producer = new Node()
producer.schedule(chain([
  "My message in a bottle...",
  IPC.Push("cbIpc1")
], "producer", true))

let consumer = new Node()
consumer.schedule(chain([
  IPC.Pop("cbIpc1"),
  Log(),
  Assert.Is("My message in a bottle...")
], "consumer", true))

if(!n.tick())
{
  throw new Error()
}

let ticks = 10
while(ticks--)
{
  if(!producer.tick()) { throw new Error() }
  if(!consumer.tick()) { throw new Error() }
}

(GetVariable "var")
(If More 10 (
  (GetVariable "var")
  (GetVariable "var")
  (GetVariable "var")
))