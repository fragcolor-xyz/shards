---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# Evolve

```clojure
(Evolve
  :Chain [(Chain)]
  :Fitness [(Chain)]
  :Population [(Int)]
  :Mutation [(Float)]
  :Crossover [(Float)]
  :Extinction [(Float)]
  :Elitism [(Float)]
  :Threads [(Int)]
  :Coroutines [(Int)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Chain | `[(Chain)]` | `None` | The chain to optimize and evolve. |
| Fitness | `[(Chain)]` | `None` | The fitness chain to run at the end of the main chain evaluation and using its last output; should output a Float fitness value. |
| Population | `[(Int)]` | `64` | The population size. |
| Mutation | `[(Float)]` | `0.2` | The rate of mutation, 0.1 = 10%. |
| Crossover | `[(Float)]` | `0.2` | The rate of crossover, 0.1 = 10%. |
| Extinction | `[(Float)]` | `0.1` | The rate of extinction, 0.1 = 10%. |
| Elitism | `[(Float)]` | `0.1` | The rate of elitism, 0.1 = 10%. |
| Threads | `[(Int)]` | `2` | The number of cpu threads to use. |
| Coroutines | `[(Int)]` | `8` | The number of coroutines to run on each thread. |


## Input

| Type | Description |
|------|-------------|
| `[(Any)]` |  |


## Output

| Type | Description |
|------|-------------|
| `[(Seq [(Float) (Chain)])]` |  |


## Examples

```clojure
(Evolve

)
```


--8<-- "includes/license.md"
