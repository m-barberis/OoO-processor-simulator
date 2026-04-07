# Source folder overview

This folder contains the simulator implementation split by responsibility.

## `main.cpp`

Program entry point.

- Reads the input program JSON (`loadProgram`)
- Decodes instruction strings into `ParsedInstruction`
- Initializes `SystemState`
- Runs the cycle-by-cycle simulation loop (`propagate`)
- Dumps one JSON snapshot per cycle and writes the output file

## `state.hpp`

Core data model for the simulator.

- Instruction structs (`ParsedInstruction`, `DecodedInstruction`)
- Pipeline/storage entries (`ActiveListEntry`, `IntegerQueueEntry`)
- Global machine state (`SystemState`) updated at every cycle
- Default initialization of register map table and free list

## `io_handler.hpp` / `io_handler.cpp`

Input/output utilities.

- Parse input JSON into instruction strings (`loadProgram`)
- Decode textual instructions into internal representation (`decodeInstructionString`)
- Convert a `SystemState` to the expected JSON format (`dumpStateToJson`)
- Save full simulation trace to disk (`saveSimulationLog`)

## `pipeline.hpp` / `pipeline.cpp`

Pipeline behavior and cycle semantics.

- Main per-cycle propagation (`propagate`) and state transfer (`latch`)
- End condition checks (`noInstruction`, `activeListIsEmpty`)
- Main stages: fetch/decode, rename/dispatch, issue, execute, commit
- Exception handling and recovery logic
- Integer queue readiness updates and forwarding-related checks

In short:

- `state.hpp` defines data
- `io_handler.*` handles JSON and instruction parsing
- `pipeline.*` implements the processor behavior
- `main.cpp` connects everything and runs the simulation
