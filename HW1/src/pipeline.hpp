#pragma once 
#include "state.hpp"
#include "io_handler.hpp"
#include <vector>
#include <string>

// Function prototypes for the pipeline stages and other helper functions
void propagate(SystemState& state); // Function to propagate the state through the pipeline stages
void latch(SystemState& current_state, SystemState& next_state); // Function to advance the clock and latch the new state for the next cycle
bool noInstruction(SystemState& state); // Function to check if there are no more instructions to fetch
bool activeListIsEmpty(SystemState& state); // Function to check if the Active List is empty
void commit(SystemState& current_state, SystemState& next_state); // Function for the commit stage of the pipeline

// Pipeline stages
void fetch_and_decode(SystemState& current_state, SystemState& next_state);
void rename_and_dispatch(SystemState& current_state, SystemState& next_state);
void issue(SystemState& current_state, SystemState& next_state);
void execute(SystemState& current_state, SystemState& next_state);

// Helpers
void handle_exception_recovery(SystemState& current_state, SystemState& next_state);
void instruction_commit(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction);
void instruction_exception(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction);