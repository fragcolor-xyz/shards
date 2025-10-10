# Shards Evaluation Runner Script

## Overview

This document describes how to use the `solution_evaluator.shs` script to evaluate LLM-generated solutions to Shards programming tasks.

## Components

The evaluation system consists of two main parts:

1. **Static Analysis**: Uses regular expressions to check code patterns
2. **Runtime Verification**: Executes the solution and verifies its output

## How to Use

### Basic Usage

```bash
# Run evaluation on a single solution
shards run_evaluation.shs --solution="path/to/solution.shs" --problem="problem_id"
```

### Complete Evaluation Process

1. **Collect Solution**: Get the solution from an LLM
2. **Save Solution**: Store it in a .shs file
3. **Run Evaluator**: Run the evaluation script
4. **Review Results**: Check the score and feedback

## Evaluation Logic

The evaluation system checks for:

### Static Analysis Checks

1. **Syntax Correctness**:
   - Proper pipe usage
   - Correct variable naming
   - Proper comment syntax
   - Capitalized Shard names

2. **Problem-Specific Patterns**:
   - Correct use of required operations
   - Proper implementation of required functionality
   - Understanding of key concepts

### Runtime Verification (to be implemented)

1. **Execution Check**:
   - Does the code run without errors?
   - Does it produce the expected output?

2. **Performance Check**:
   - Is the solution efficient?
   - Does it follow best practices?

## Scoring System

The evaluator produces:

1. A raw score and maximum possible score
2. A percentage score
3. Detailed feedback for each criterion

## Extending the Evaluator

To add tests for new problems:

1. Create a new `@template(check-problem-name [code] {...})` template
2. Add specific pattern checks using `@has-pattern`
3. Add the problem ID to the match statement in `evaluate-solution`

## Runtime Evaluation Integration

To integrate runtime evaluation:

1. Save the solution to a temporary file
2. Execute it with the Shards interpreter
3. Capture the output
4. Compare with expected output patterns

```shards
// Example runtime evaluation addition
@template(run-and-verify [code expected-output] {
  code | WriteFile("temp_solution.shs")
  "shards temp_solution.shs" | Shell.Execute = actual-output
  actual-output | Regex.Search(Regex: expected-output) | Not | IsEmpty
})
```

## Future Improvements

1. Add detailed runtime testing
2. Implement deeper semantic analysis
3. Add support for comparing multiple solutions
4. Create a web interface for interactive evaluation
5. Generate detailed reports with improvement suggestions