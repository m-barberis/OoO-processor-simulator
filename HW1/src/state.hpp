#pragma once
#include <vector>
#include <array>
#include <string>
#include <cstdint>

// Struct for an entry in the Active List
struct ActiveListEntry {
    bool Done = false;
    bool Exception = false;
    uint32_t LogicalDestination = 0;
    uint32_t OldDestination = 0;
    uint64_t PC = 0;
};

// Struct for an entry in the Integer Queue
struct IntegerQueueEntry {
    uint32_t DestRegister = 0;
    
    bool OpAIsReady = false;
    uint32_t OpARegTag = 0;
    uint64_t OpAValue = 0;
    
    bool OpBIsReady = false;
    uint32_t OpBRegTag = 0;
    uint64_t OpBValue = 0;
    
    std::string OpCode = "";
    uint64_t PC = 0;
};

// Global system state struct
struct SystemState {
    uint64_t PC = 0;
    std::array<uint64_t, 64> PhysicalRegisterFile = {0};
    std::vector<uint64_t> DecodedPCs;
    
    bool Exception = false; 
    uint64_t ExceptionPC = 0;
    std::array<uint32_t, 32> RegisterMapTable;
    std::vector<uint32_t> FreeList;
    std::array<bool, 64> BusyBitTable = {false};
    
    std::vector<ActiveListEntry> ActiveList;
    std::vector<IntegerQueueEntry> IntegerQueue;

    // Constructor to initialize the system state according to the specifications
    SystemState() {
        // Initialization of the Register Map Table: logical registers map to the same physical registers
        for (uint32_t i = 0; i < 32; ++i) {
            RegisterMapTable[i] = i;
        }
        // Initialization of the Free List: contains the physical registers from p32 to p63
        for (uint32_t i = 32; i < 64; ++i) {
            FreeList.push_back(i);
        }
    }
};