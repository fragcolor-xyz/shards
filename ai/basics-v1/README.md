# Shards Programming Language Evaluation Dataset

## Overview

This directory contains a collection of evaluation materials for testing understanding of the Shards programming language. The dataset is designed to help assess whether language models or humans comprehend the key concepts and syntax of Shards.

## Contents

### Evaluation Scripts (`scripts/`)

Working Shards scripts demonstrating various language features:

1. `variable_assignment.shs` - Variable assignments and updates
2. `control_flow.shs` - Control flow structures (If, When, Repeat, ForEach, Match, Once)
3. `data_types.shs` - Basic data types and working with sequences/tables
4. `string_operations.shs` - String joining and formatting
5. `wires_and_state.shs` - Wire definitions and state management
6. `operation_grouping.shs` - Operation grouping with parentheses
7. `error_handling_simple.shs` - Error handling with Maybe
8. `templates_basic.shs` - Creating and using templates

### Question-Answer Pairs (`qa/`)

Individual JSON files with questions and detailed answers about Shards concepts:

1. `variable_assignment.json` - Variable assignment patterns
2. `control_flow.json` - Control flow structures
3. `data_types.json` - Data types and collections
4. `string_operations.json` - String operations
5. `wires_and_state.json` - Wire definitions and state management
6. `operation_grouping.json` - Operation grouping and execution order
7. `error_handling.json` - Error handling with Maybe
8. `templates.json` - Templates for code reuse

### Combined Dataset

`shards_evaluation_dataset.json` - All Q&A pairs in a single JSON array for easy use in evaluation scenarios.

## Usage

This dataset can be used to:

1. Test knowledge of Shards programming language concepts
2. Evaluate language model understanding of Shards syntax
3. Create educational materials for learning Shards
4. Benchmark training progress for Shards-specific language models

## Example

Each Q&A pair follows this format:

```json
{
  "question": "How do you assign values to variables in Shards?",
  "answer": "In Shards, there are two types of variable assignments: ..." 
}
```

All scripts have been tested and verified to work correctly with the Shards interpreter.