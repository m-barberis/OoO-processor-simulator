
#include "state.hpp"
#include "io_handler.hpp"
#include "pipeline.hpp"
#include <vector>
#include <string>

int main() {
    // Load the program instructions from the input JSON file
    std::vector<std::string> instructionStrings = loadProgram("input.json");
    
    // Parse the instruction strings into ParsedInstruction structs
    std::vector<ParsedInstruction> instructions;
    for (const auto& inst_str : instructionStrings) {
        instructions.push_back(decodeInstructionString(inst_str));
    }
    
    // Initialize the system state
    SystemState state;
    state.instructions = instructions; // Store parsed instructions in the system state

    bool backpressure_on = false;
    state.backpressure_on = backpressure_on;
    
    // Vector to hold the simulation log (JSON states for each cycle)
    std::vector<json> simulationLog;

    simulationLog.push_back(dumpStateToJson(state)); // Log the initial state before starting the simulation
    
    // Main simulation loop 
    // the loop for cycle-by-cycle iterations.
    while(!noInstruction(state)){
    
    // do propagation
    // if you have multiple modules, propagate each of them
    propagate(state);
    
    // latch stage moved at the end of propagate 

    // dump the state
    simulationLog.push_back(dumpStateToJson(state));

    }
    // save the output JSON log
    saveSimulationLog(simulationLog, "output.json");


}