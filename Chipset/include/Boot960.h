/*
i960SxChipset_Type103
Copyright (c) 2022-2023, Joshua Scoggins
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
//
// Created by jwscoggins on 1/7/23.
//

#ifndef CHIPSET_BOOT960_H
#define CHIPSET_BOOT960_H
#include <stdint.h>

struct FaultRecord {
    uint32_t pc;
    uint32_t ac;
    uint32_t type;
    uint32_t* addr;
} __attribute((packed));
struct FaultTableEntry {
    typedef void(*FaultOperation)();
    uint32_t handlerRaw;
    int32_t magicNumber;
    inline FaultOperation getFaultFunction() const noexcept { return reinterpret_cast<FaultOperation>(handlerRaw & 0xFFFFFFFC); }
    inline uint8_t getProcedureKind() const noexcept { return static_cast<uint8_t>(handlerRaw & 0x3); }
    inline bool isLocalProcedure() const noexcept { return getProcedureKind() == 0; }
    inline bool isSystemProcedure() const noexcept { return getProcedureKind() == 0x2 && getMagicNumber() == 0x0000027F; }
    inline bool isTraceFaultHandler() const noexcept { return getProcedureKind() == 0x2 && getMagicNumber() == 0x000002BF; }
    inline int32_t getMagicNumber() const noexcept { return magicNumber; }
} __attribute((packed));
struct FaultTable {
    FaultTableEntry entries[32];
} __attribute((packed));
struct InterruptTable {
    uint32_t pendingPriorities;
    uint32_t pendingInterrupts[8];
    void (*interruptProcedures[248])();
} __attribute((packed));
struct InterruptRecord {
    uint32_t pc;
    uint32_t ac;
    uint8_t vectorNumber;
} __attribute((packed));
struct SATEntry {
    typedef void (*Executable)();
    enum Kind {
        LocalProcedure,
        Reserved0,
        SupervisorProcedure,
        Reserved1,
        Invalid = Reserved1,
    };
    uint32_t raw;
    inline Executable asFunction() const noexcept {
        // drop the lowest tag bits to make sure that it is the right location
        return reinterpret_cast<Executable>(raw & 0xFFFFFFFC);
    }
    inline Kind getKind() const noexcept {
        return static_cast<Kind>(raw & 0x3);
    }
} __attribute((packed));
struct SysProcTable {
    uint32_t reserved[3];
    void* supervisorStack;
    uint32_t preserved[8];
    SATEntry entries[260];
    typedef SATEntry::Executable FunctionBody;

    inline FunctionBody getEntry(uint32_t index) noexcept {
        if (index > 259) {
            return nullptr;
        } else {
            return entries[index].asFunction();
        }
    }
    inline SATEntry::Kind getEntryKind(uint32_t index) noexcept {
        if (index > 259) {
            return SATEntry::Reserved1;
        } else {
            return entries[index].getKind();
        }
    }
} __attribute((packed));
struct PRCB {
    uint32_t reserved0;
    uint32_t magicNumber;
    uint32_t reserved1[3];
    InterruptTable* theInterruptTable;
    void* interruptStack;
    uint32_t reserved2;
    uint32_t magicNumber1;
    uint32_t magicNumber2;
    FaultTable* theFaultTable;
    uint32_t reserved3[32];
} __attribute((packed));
/**
 * @brief Describes an i960Sx boot structure that can be embedded into flash
 */
struct SystemAddressTable {
    SystemAddressTable* satPtr;
    PRCB* thePRCB;
    uint32_t checkWord;
    void (*firstInstruction)();
    uint32_t checkWords[4];
    uint32_t preserved0[22];
    SysProcTable* theProcTable0;
    uint32_t magicNumber0;
    uint32_t preserved1[2];
    SystemAddressTable* offset0;
    uint32_t magicNumber1;
    uint32_t preserved2[2];
    SysProcTable* theProcTable1;
    uint32_t magicNumber2;
    uint32_t preserved3[2];
    SysProcTable* traceTable;
    uint32_t magicNumber3;
} __attribute((packed));
#endif //CHIPSET_BOOT960_H
