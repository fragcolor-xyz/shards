---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Collections

## assoc

```
(assoc map key val) (assoc map key val & kvs)
```

assoc[iate]. When applied to a map, returns a new map of the same (hashed/sorted) type, that contains the mapping of key(s) to val(s). When applied to a vector, returns a new vector that contains val at index. Note - index must be <= (count vector).

=== "Code"

    ```clojure linenums="1"
    (assoc {} :apples 1 :oranges 3)
    ;;=> {:apples 1}
    ```

## concat

```
(concat) (concat x) (concat x y) (concat x y & zs)
```

Returns a sequence representing the concatenation of the elements in the supplied colls.

=== "Code"

    ```clojure linenums="1"
    (concat ["apple" "mango"] ["banana"])
    ;; => ("apple" "mango" "banana")
    ```

## conj

```
(conj coll x) (conj coll x & xs)
```

conj[oin]. Returns a new collection with the xs 'added'. The 'addition' may happen at different 'places' depending on the concrete type of coll.

=== "Code"

    ```clojure linenums="1"
    (conj ["apple" "mango"] "cherry")
    ;; => ["apple" "mango" "cherry"]
    ```

## cons

```
(cons x seq)
```

Constructs a list recursively using the pattern `(cons element rest-of-list)`.

=== "Code"

    ```clojure linenums="1"
    (cons "apple" (cons "mango" (cons "cherry" nil)))
    ;; => ("apple" "mango" "cherry")
    ```

## contains?

```
(contains? hashmap key)
```

Checks whether a hashmap contains a record with the given key. Given key can be either a string or a keyword.

=== "Code"

    ```clojure linenums="1"
    (contains? {:apples 4} :apples)
    ;;=> true
    (contains? {:apples 4} :oranges)
    ;;=> false
    ```

## count

```
(count coll)
```

Counts the number of elements in a coll.

=== "Code"

    ```clojure linenums="1"
    (count [1 2 4])
    ;;=> 3
    ```

## dissoc

```
(dissoc map) (dissoc map key) (dissoc map key & ks)
```

=== "Code"

    ```clojure linenums="1"
    (dissoc {:oranges 2 :apples 3} :apples) ;; => {:oranges 2}
    ```

## empty?

```
(empty? coll)
```

Checks whether coll has no elements.

=== "Code"

    ```clojure linenums="1"
    (empty? [1 2 3]) ;;=> false
    (empty? []) ;;=> true
    (empty? nil) ;;=> true
    ```

## get

```
(get map key) (get map key not-found)
```

Returns the value mapped to key, not-found or nil if key not present.

=== "Code"

    ```clojure linenums="1"
    (get {:apples 1} :apples)
    ;; => 1
    ```

## first

```
(first coll)
```

Returns the first item in the collection. If coll is nil, returns nil.

=== "Code"

    ```clojure linenums="1"
    (first ["apple" "mango"])
    ;; => "apple"
    ```

## keys

```
(keys map)
```

Gets a list of all the keys from a hashmap.

=== "Code"

    ```clojure linenums="1"
    (keys {:oranges 2 :apples 3})
    ;; => (:apples :oranges)
    ```

## list

```
(list & items)
```

Creates a new list containing the items.

=== "Code"

    ```clojure linenums="1"
    (list 1 2 3)
    ;; => (1 2 3)
    ```

## hash-map

```
(hash-map) (hash-map & keyvals)
```

Returns a new hash map with supplied mappings. If any keys are
equal, they are handled as if by repeated uses of assoc.

=== "Code"

    ```clojure linenums="1"
    (hash-map :apples 1 :oranges 2)
    ;;=> {:apples 1 :oranges 2}
    ```


## map

```
(map f coll)
```

Returns a sequence consisting of the result of applying f to
the first item of coll, followed by applying f to the second item of coll, until coll is exhausted.

=== "Code"

    ```clojure linenums="1"
    (map inc [1 2 3 4 5])
    ;; => (2 3 4 5 6)
    ```

## nth

```
(nth coll index)
```

Returns the value at the index of coll.

=== "Code"

    ```clojure linenums="1"
    (nth ["a" "b" "c" "d"] 0)
    ;; => "a"
    ```

## rest

```
(rest coll)
```

Returns a possibly empty sequence of the items after the first.

=== "Code"

    ```clojure linenums="1"
    (rest ["apple" "mango" "cherry"])
    ;;=> ("mango" "cherry")
    (rest []) ;;=> ()
    (rest nil) ;;=> ()
    ```

## reverse

```
(reverse coll)
```

Returns a sequence of the items in coll in reverse order.

=== "Code"

    ```clojure linenums="1"
    (reverse ["apple" "mango" "banana"])
    ;;=> ("banana" "mango" "apple")
    ```

## seq

```
(seq coll)
```

Returns a sequence on the collection. If the collection is empty, returns nil. (seq nil) returns nil. `seq` also works on Strings.

=== "Code"

    ```clojure linenums="1"
    (seq "abc") ;;=> ("a" "b" "c")
    (seq []) ;;=> nil
    (seq nil) ;;=> nil
    ```

## vals

```
(vals map)
```

Returns a sequence of the map's values.

=== "Code"

    ```clojure linenums="1"
    (vals {:apples 3 :mangos 2})
    ;;=> (3 2)
    ```

## vector

```
(vector) (vector a) (vector a b) (vector a b c) (vector a b c d) (vector a b c d e) (vector a b c d e f) (vector a b c d e f & args)
```

Creates a new vector containing the args.

=== "Code"

    ```clojure linenums="1"
    (vector "mango" "banana" "peach")
    ;;=> ["mango" "banana" "peach"]
    ```


--8<-- "includes/license.md"
