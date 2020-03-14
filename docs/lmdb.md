# LMDB blocks

## `DB.Put`

Straight forward

```clojure=
(...) ; the value to put
(DB.Put "Key")
```

## `DB.Get`

Straight forward

```clojure=
(DB.Get "Key")
(ExpectInt) ; must be guarded
(...) ; the int value
```

## `DB.Iterate`

It will take a *partial key*, and will output a new value for each iteration of the block. When a new value is not available it will *STOP* the chain.

```clojure=
(DB.Iterate "PartialKey")
(ExpectInt) ; must be guarded
(...) ; the int value
```

```python=
.DB_Iterate("PartialKey")
.ExpectInt()
.Log("We got a int:")
```

## `DB.Delete`

* When a key is provided it will directly delete that key.
* When no key is provided it will delete the current iterated value in the context (needs a previous `DB.Iterate` in the chain).

```clojure=
(DB.Iterate "PartialKey")
(DB.Delete) ; will delete the iterated current record
```

```clojure=
(DB.Delete "Key") ; will delete 'Key'
```
