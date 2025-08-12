The shard follows a process for evolving the wire specified.
1. Population Initialization
  - It creates a population of individuals, each containing a copy of the base wire to be evolved.
  - The population size is determined by the "Population" parameter.
2. Evaluation
  - For each individual in the population, it runs the base wire and then the fitness wire.
  - The fitness wire should output a float value representing the fitness of the individual (higher is better).
3. Selection
  - It sorts the population based on their fitness scores.
  - The best individuals are kept (elitism), while the worst are marked for replacement.
4. Crossover
  - It performs crossover operations between individuals, combining their "genetic material" (wire configurations).
  - The crossover rate is controlled by the "Crossover" parameter.
5. Mutation
  - It applies mutations to individuals, making small random changes to their wire configurations.
  - The mutation rate is controlled by the "Mutation" parameter.
6. Iteration
  - Steps 2-5 are repeated for multiple generations (each call of the Evolve shard represents one generation).
7. Output
  - After each generation, it returns the best individual's fitness score and wire configuration.

For a wire to be able to evolve, it must be using the Mutant or DShard shards.

