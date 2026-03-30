#pragma once 
#include "state.hpp"
#include "io_handler.hpp"
#include <vector>
#include <string>

// Function prototypes for the pipeline stages and other helper functions
void propagate(); // Function to propagate the state through the pipeline stages
void latch(); // Function to advance the clock and latch the new state for the next cycle
bool noInstruction(); // Function to check if there are no more instructions to fetch
bool activeListIsEmpty(); // Function to check if the Active List is empty