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
| Chain | `[(Chain)]` |  | The chain to optimize and evolve. |
| Fitness | `[(Chain)]` |  | The fitness chain to run at the end of the main chain evaluation and using its last output; should output a Float fitness value. |
| Population | `[(Int)]` |  | The population size. |
| Mutation | `[(Float)]` |  | The rate of mutation, 0.1 = 10%. |
| Crossover | `[(Float)]` |  | The rate of crossover, 0.1 = 10%. |
| Extinction | `[(Float)]` |  | The rate of extinction, 0.1 = 10%. |
| Elitism | `[(Float)]` |  | The rate of elitism, 0.1 = 10%. |
| Threads | `[(Int)]` |  | The number of cpu threads to use. |
| Coroutines | `[(Int)]` |  | The number of coroutines to run on each thread. |


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
