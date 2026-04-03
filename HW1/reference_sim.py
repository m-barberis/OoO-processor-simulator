#!/usr/bin/env python3
"""
Cycle-accurate Python reference simulator for CS470 HW1 Out-of-Order processor.

Mirrors the EXACT stage evaluation order of pipeline.cpp:
  propagate() => commit -> execute -> issue(next_state) -> rename_and_dispatch -> fetch_and_decode

All arithmetic is unsigned 64-bit (uint64_t), matching the C++ implementation.
"""

import json
import sys
import copy
from dataclasses import dataclass, field
from typing import List

UINT64_MASK = 0xFFFF_FFFF_FFFF_FFFF


# ── Data structures ────────────────────────────────────────────────────────────

@dataclass
class ParsedInstruction:
    opcode: str
    dest:   int
    opA:    int
    opB:    int   # register index OR immediate (signed int from parse)
    is_addi: bool


@dataclass
class DecodedInstruction:
    PC:   int
    inst: ParsedInstruction


@dataclass
class ActiveListEntry:
    Done:               bool = False
    Exception:          bool = False
    LogicalDestination: int  = 0
    OldDestination:     int  = 0
    PC:                 int  = 0


@dataclass
class IQEntry:
    DestRegister: int  = 0
    OpAIsReady:   bool = False
    OpARegTag:    int  = 0
    OpAValue:     int  = 0
    OpBIsReady:   bool = False
    OpBRegTag:    int  = 0
    OpBValue:     int  = 0
    OpCode:       str  = ""
    PC:           int  = 0


class SystemState:
    """Mirrors SystemState in state.hpp, constructor initialises identically."""

    def __init__(self):
        self.PC:                    int              = 0
        self.PhysicalRegisterFile:  List[int]        = [0] * 64
        self.DecodedPCs:            List[int]        = []
        self.Exception:             bool             = False
        self.ExceptionPC:           int              = 0
        self.RegisterMapTable:      List[int]        = list(range(32))  # RMT[i]=i
        self.FreeList:              List[int]        = list(range(32, 64))
        self.BusyBitTable:          List[bool]       = [False] * 64
        self.ActiveList:            List[ActiveListEntry] = []
        self.IntegerQueue:          List[IQEntry]    = []
        self.ReadyInstructions:     List[IQEntry]    = []   # internal
        self.ExecutionQueue:        List[IQEntry]    = []   # internal
        self.DecodedInstructionQueue: List[DecodedInstruction] = []  # internal
        self.backpressure_on:       bool             = False
        self.instructions:          List[ParsedInstruction] = []  # static

    def copy(self) -> "SystemState":
        s = SystemState()
        s.PC                     = self.PC
        s.PhysicalRegisterFile   = self.PhysicalRegisterFile[:]
        s.DecodedPCs             = self.DecodedPCs[:]
        s.Exception              = self.Exception
        s.ExceptionPC            = self.ExceptionPC
        s.RegisterMapTable       = self.RegisterMapTable[:]
        s.FreeList               = self.FreeList[:]
        s.BusyBitTable           = self.BusyBitTable[:]
        s.ActiveList             = [copy.copy(e) for e in self.ActiveList]
        s.IntegerQueue           = [copy.copy(e) for e in self.IntegerQueue]
        s.ReadyInstructions      = [copy.copy(e) for e in self.ReadyInstructions]
        s.ExecutionQueue         = [copy.copy(e) for e in self.ExecutionQueue]
        s.DecodedInstructionQueue = [copy.copy(e) for e in self.DecodedInstructionQueue]
        s.backpressure_on        = self.backpressure_on
        s.instructions           = self.instructions   # static list, safe to share
        return s


# ── Parsing ────────────────────────────────────────────────────────────────────

def parse_instruction(s: str) -> ParsedInstruction:
    """Mirrors decodeInstructionString() in io_handler.cpp."""
    s = s.replace(',', ' ')
    parts = s.split()
    opcode  = parts[0]
    dest    = int(parts[1][1:])
    opA     = int(parts[2][1:])
    if opcode == 'addi':
        opB     = int(parts[3])   # immediate, may be negative
        is_addi = True
    else:
        opB     = int(parts[3][1:])
        is_addi = False
    return ParsedInstruction(opcode=opcode, dest=dest, opA=opA, opB=opB, is_addi=is_addi)


# ── State serialisation ────────────────────────────────────────────────────────

def dump_state(state: SystemState) -> dict:
    """Mirrors dumpStateToJson() in io_handler.cpp."""
    al_json = [
        {
            "Done":               e.Done,
            "Exception":          e.Exception,
            "LogicalDestination": e.LogicalDestination,
            "OldDestination":     e.OldDestination,
            "PC":                 e.PC,
        }
        for e in state.ActiveList
    ]

    iq_json = []
    for e in state.IntegerQueue:
        opcode = "add" if e.OpCode == "addi" else e.OpCode
        iq_json.append({
            "DestRegister": e.DestRegister,
            "OpAIsReady":   e.OpAIsReady,
            "OpARegTag":    e.OpARegTag,
            "OpAValue":     e.OpAValue,
            "OpBIsReady":   e.OpBIsReady,
            "OpBRegTag":    e.OpBRegTag,
            "OpBValue":     e.OpBValue,
            "OpCode":       opcode,
            "PC":           e.PC,
        })

    return {
        "PC":                   state.PC,
        "PhysicalRegisterFile": list(state.PhysicalRegisterFile),
        "DecodedPCs":           list(state.DecodedPCs),
        "Exception":            state.Exception,
        "ExceptionPC":          state.ExceptionPC,
        "RegisterMapTable":     list(state.RegisterMapTable),
        "FreeList":             list(state.FreeList),
        "BusyBitTable":         list(state.BusyBitTable),
        "ActiveList":           al_json,
        "IntegerQueue":         iq_json,
    }


# ── Termination check ──────────────────────────────────────────────────────────

def no_instruction(state: SystemState) -> bool:
    """Mirrors noInstruction() in pipeline.cpp, including !Exception guard."""
    return (
        len(state.DecodedPCs) == 0
        and state.PC >= len(state.instructions)
        and len(state.ActiveList) == 0
        and not state.Exception
    )


# ── Pipeline stages ────────────────────────────────────────────────────────────

def _instruction_commit(next_state: SystemState, entry: ActiveListEntry):
    """Mirrors instruction_commit() — returns OldDestination to FreeList."""
    next_state.FreeList.append(entry.OldDestination)
    next_state.ActiveList.pop(0)


def _instruction_exception(next_state: SystemState, entry: ActiveListEntry):
    """Mirrors instruction_exception() — sets Exception and clears all buffers."""
    next_state.Exception  = True
    next_state.ExceptionPC = entry.PC
    next_state.IntegerQueue.clear()
    next_state.ReadyInstructions.clear()
    next_state.ExecutionQueue.clear()
    next_state.DecodedInstructionQueue.clear()
    next_state.DecodedPCs.clear()


def commit(current: SystemState, nxt: SystemState):
    """
    Mirrors commit() in pipeline.cpp.
    Reads current.ActiveList (head first), writes to nxt.
    Stops at first not-Done entry OR after 4 commits OR on exception.
    """
    to_commit = min(4, len(current.ActiveList))
    for i in range(to_commit):
        entry = current.ActiveList[i]
        if entry.Done and entry.Exception:
            _instruction_exception(nxt, entry)
            break
        elif entry.Done:
            _instruction_commit(nxt, entry)
        else:
            break


def execute(current: SystemState, nxt: SystemState):
    """
    Mirrors execute() in pipeline.cpp.
    Reads current.ExecutionQueue; writes PRF, BusyBitTable, AL.Done in nxt.
    Then latches nxt.ExecutionQueue = current.ReadyInstructions.
    """
    if nxt.Exception:
        return

    nxt.ExecutionQueue.clear()

    for instr in current.ExecutionQueue:
        caused_exception = False

        # ── Compute result ──────────────────────────────────────────────────
        if instr.OpCode in ('add', 'addi'):
            nxt.PhysicalRegisterFile[instr.DestRegister] = (
                instr.OpAValue + instr.OpBValue) & UINT64_MASK

        elif instr.OpCode == 'sub':
            nxt.PhysicalRegisterFile[instr.DestRegister] = (
                instr.OpAValue - instr.OpBValue) & UINT64_MASK

        elif instr.OpCode == 'mulu':
            nxt.PhysicalRegisterFile[instr.DestRegister] = (
                instr.OpAValue * instr.OpBValue) & UINT64_MASK

        elif instr.OpCode == 'divu':
            if instr.OpBValue != 0:
                nxt.PhysicalRegisterFile[instr.DestRegister] = (
                    instr.OpAValue // instr.OpBValue)
            else:
                caused_exception = True
                for ale in nxt.ActiveList:
                    if ale.PC == instr.PC:
                        ale.Exception = True
                        break

        elif instr.OpCode == 'remu':
            if instr.OpBValue != 0:
                nxt.PhysicalRegisterFile[instr.DestRegister] = (
                    instr.OpAValue % instr.OpBValue)
            else:
                caused_exception = True
                for ale in nxt.ActiveList:
                    if ale.PC == instr.PC:
                        ale.Exception = True
                        break

        # ── Busy bit (only clear if no exception) ──────────────────────────
        if not caused_exception:
            nxt.BusyBitTable[instr.DestRegister] = False

        # ── Mark Done in Active List ────────────────────────────────────────
        for ale in nxt.ActiveList:
            if ale.PC == instr.PC:
                ale.Done = True
                break

    # Latch next execution wave (will execute next cycle)
    nxt.ExecutionQueue = [copy.copy(e) for e in current.ReadyInstructions]


def _update_integer_queue(nxt: SystemState):
    """
    Mirrors updateIntegerQueue() — wakes up IQ entries whose source regs
    are no longer busy (forwarding path from execute).
    Operates entirely on nxt.
    """
    for entry in nxt.IntegerQueue:
        if not entry.OpAIsReady:
            if not nxt.BusyBitTable[entry.OpARegTag]:
                entry.OpAIsReady = True
                entry.OpAValue   = nxt.PhysicalRegisterFile[entry.OpARegTag]
        if not entry.OpBIsReady:
            if not nxt.BusyBitTable[entry.OpBRegTag]:
                entry.OpBIsReady = True
                entry.OpBValue   = nxt.PhysicalRegisterFile[entry.OpBRegTag]


def issue(nxt: SystemState):
    """
    Mirrors issue(next_state) in pipeline.cpp — operates ONLY on nxt.
    This is called AFTER execute, so it sees updated busy bits from this cycle.
    """
    if nxt.Exception:
        return

    nxt.ReadyInstructions.clear()
    _update_integer_queue(nxt)

    ready = [e for e in nxt.IntegerQueue if e.OpAIsReady and e.OpBIsReady]
    ready.sort(key=lambda e: e.PC)   # oldest first

    to_issue = ready[:4]
    issued_pcs = {e.PC for e in to_issue}
    nxt.IntegerQueue = [e for e in nxt.IntegerQueue if e.PC not in issued_pcs]
    nxt.ReadyInstructions = to_issue


def rename_and_dispatch(current: SystemState, nxt: SystemState):
    """
    Mirrors rename_and_dispatch() — reads current.DecodedInstructionQueue,
    writes new IQ entries / AL entries / RMT / BusyBitTable to nxt.
    """
    if nxt.Exception:
        return

    # Backpressure check (same conditions as C++)
    backpressure = (
        len(nxt.ActiveList)   > 28 or
        len(nxt.FreeList)     < 4  or
        len(nxt.IntegerQueue) > 28
    )
    nxt.backpressure_on = backpressure

    if backpressure:
        return

    to_dispatch = min(4, len(current.DecodedInstructionQueue))

    # Remove dispatched instructions from nxt's decoded queue
    nxt.DecodedInstructionQueue = nxt.DecodedInstructionQueue[to_dispatch:]

    for i in range(to_dispatch):
        di   = current.DecodedInstructionQueue[i]
        inst = di.inst

        if not nxt.FreeList:
            break

        phys_dest = nxt.FreeList.pop(0)
        old_dest  = nxt.RegisterMapTable[inst.dest]

        iqe = IQEntry()
        iqe.OpCode      = inst.opcode
        iqe.PC          = di.PC
        iqe.DestRegister = phys_dest

        # Read operand A  (BEFORE updating RMT for dest)
        iqe.OpARegTag = nxt.RegisterMapTable[inst.opA]
        if nxt.BusyBitTable[iqe.OpARegTag]:
            iqe.OpAIsReady = False
        else:
            iqe.OpAIsReady = True
            iqe.OpAValue   = nxt.PhysicalRegisterFile[iqe.OpARegTag]

        # Read operand B
        if not inst.is_addi:
            iqe.OpBRegTag = nxt.RegisterMapTable[inst.opB]
            if nxt.BusyBitTable[iqe.OpBRegTag]:
                iqe.OpBIsReady = False
            else:
                iqe.OpBIsReady = True
                iqe.OpBValue   = nxt.PhysicalRegisterFile[iqe.OpBRegTag]
        else:
            # Sign-extend immediate to uint64, matching C++ cast chain
            iqe.OpBIsReady = True
            iqe.OpBRegTag  = 0          # unused for addi
            iqe.OpBValue   = inst.opB & UINT64_MASK   # handles negatives

        # Now update RMT and BusyBitTable for dest
        nxt.RegisterMapTable[inst.dest]  = phys_dest
        nxt.BusyBitTable[phys_dest]      = True

        nxt.IntegerQueue.append(iqe)

        ale = ActiveListEntry(
            LogicalDestination = inst.dest,
            OldDestination     = old_dest,
            PC                 = di.PC,
        )
        nxt.ActiveList.append(ale)


def fetch_and_decode(current: SystemState, nxt: SystemState):
    """Mirrors fetch_and_decode() in pipeline.cpp."""
    if nxt.Exception:
        nxt.PC = 0x10000   # jump to exception handler
        nxt.DecodedInstructionQueue.clear()
    elif current.backpressure_on:
        pass   # stall — do nothing
    else:
        fetch_capacity = 4 - len(nxt.DecodedInstructionQueue)
        if fetch_capacity > 0:
            to_fetch = min(
                fetch_capacity,
                len(current.instructions) - current.PC
            )
            for i in range(to_fetch):
                nxt.DecodedInstructionQueue.append(
                    DecodedInstruction(
                        PC   = current.PC + i,
                        inst = current.instructions[current.PC + i],
                    )
                )
            nxt.PC += to_fetch

    # DecodedPCs always mirrors DecodedInstructionQueue
    nxt.DecodedPCs = [di.PC for di in nxt.DecodedInstructionQueue]


def handle_exception_recovery(current: SystemState, nxt: SystemState):
    """
    Mirrors handle_exception_recovery() in pipeline.cpp.
    Drains up to 4 entries per cycle from the BACK of the Active List,
    restoring RMT / FreeList / BusyBitTable.
    Exception transitions to False only when AL was ALREADY empty (else-branch).
    """
    if current.ActiveList:
        to_restore = min(4, len(nxt.ActiveList))
        for _ in range(to_restore):
            entry     = nxt.ActiveList[-1]
            phys_dest = nxt.RegisterMapTable[entry.LogicalDestination]
            nxt.FreeList.append(phys_dest)
            nxt.RegisterMapTable[entry.LogicalDestination] = entry.OldDestination
            nxt.BusyBitTable[phys_dest] = False
            nxt.ActiveList.pop()
        # NOTE: keep Exception=True — the reference requires one more cycle with
        # empty AL + Exception=True before transitioning back to normal mode.
    else:
        # AL was already empty at the START of this cycle → exit recovery
        nxt.Exception = False
        # ExceptionPC preserved intentionally


# ── Top-level propagation ──────────────────────────────────────────────────────

def propagate(state: SystemState) -> SystemState:
    """
    Mirrors propagate() in pipeline.cpp.
    Creates next_state as a copy, applies stages in the correct order, returns it.
    """
    nxt = state.copy()

    if state.Exception:
        handle_exception_recovery(state, nxt)
    else:
        # Exact order from C++:
        commit(state, nxt)
        execute(state, nxt)
        issue(nxt)                          # only next_state (matches C++ change)
        rename_and_dispatch(state, nxt)
        fetch_and_decode(state, nxt)

    return nxt   # latch


# ── Simulation loop ────────────────────────────────────────────────────────────

def simulate(instructions: List[ParsedInstruction]) -> List[dict]:
    state = SystemState()
    state.instructions = instructions

    log  = [dump_state(state)]   # log initial state (cycle 0)
    cycle = 0

    while not no_instruction(state) and cycle < 1000:
        state = propagate(state)
        log.append(dump_state(state))
        cycle += 1

    return log


# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print("Usage: reference_sim.py <input.json> [output.json]")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        instruction_strings = json.load(f)

    instructions = [parse_instruction(s) for s in instruction_strings]
    log = simulate(instructions)

    out_file = sys.argv[2] if len(sys.argv) > 2 else "ref_output.json"
    with open(out_file, 'w') as f:
        json.dump(log, f, indent=4)

    print(f"Simulated {len(log)} cycles -> {out_file}")


if __name__ == '__main__':
    main()
