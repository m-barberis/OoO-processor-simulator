#pragma once
#include "state.hpp"
#include "io_handler.hpp"
#include <vector>
#include <string>

int main() {
    // Load the program instructions from the input JSON file
    std::vector<std::string> instructionStrings = loadProgram("input.json");
    
    // Decode the instruction strings into ParsedInstruction structs
    std::vector<ParsedInstruction> instructions;
    for (const auto& inst_str : instructionStrings) {
        instructions.push_back(decodeInstructionString(inst_str));
    }
    
    // Initialize the system state
    SystemState state;
    
    // Vector to hold the simulation log (JSON states for each cycle)
    std::vector<json> simulationLog;
    
    // Main simulation loop 

}