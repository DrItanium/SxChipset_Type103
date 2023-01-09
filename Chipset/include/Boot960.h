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
#include "Types.h"
namespace i960
{
    struct FaultRecord
    {
        uint32_t pc;
        uint32_t ac;
        uint32_t type;
        uint32_t addr;
    } __attribute((packed));
    struct FaultTableEntry
    {
        uint32_t handlerRaw;
        int32_t magicNumber;
    } __attribute((packed));
    struct FaultTable
    {
        FaultTableEntry entries[32];
    } __attribute((packed));
    struct InterruptTable
    {
        uint32_t pendingPriorities;
        uint32_t pendingInterrupts[8];
        uint32_t interruptProcedures[248];
    } __attribute((packed));
    struct InterruptRecord
    {
        uint32_t pc;
        uint32_t ac;
        uint8_t vectorNumber;
    } __attribute((packed));
    struct SysProcTable
    {
        uint32_t reserved[3];
        uint32_t supervisorStack;
        uint32_t preserved[8];
        uint32_t entries[260];
    } __attribute((packed));
    struct PRCB
    {
        uint32_t reserved0;
        uint32_t magicNumber;
        uint32_t reserved1[3];
        uint32_t theInterruptTable;
        uint32_t interruptStack;
        uint32_t reserved2;
        uint32_t magicNumber1;
        uint32_t magicNumber2;
        uint32_t theFaultTable;
        uint32_t reserved3[32];
    } __attribute((packed));
    struct [[gnu::packed]] SegmentDescriptor {
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
        uint32_t address = 0;
        uint32_t flags = 0;
    };
    struct [[gnu::packed]] SystemAddressTable
    {
        // this merger of the check words and SAT is Intel exploiting the fact that the segment descriptor table is actually ignored
        // in the Sx and Kx variants. You can place the SAT on 16-byte aligned boundaries anywhere in memory. I am going to be moving this
        // to be on the safe side.
        SegmentDescriptor entries[11];
    };
    static_assert(sizeof(SystemAddressTable) == 176);
    constexpr uint32_t
    computeCS1(uint32_t sat, uint32_t prcb, uint32_t first_ip) noexcept {
        return -(sat + prcb + first_ip);
    }

/**
 * @brief Describes an i960Sx boot structure that can be embedded into flash
 */
    struct [[gnu::packed]] CoreInitializationBlock
    {
        explicit constexpr CoreInitializationBlock(uint32_t satBase, uint32_t prcbPointer, uint32_t checkWord, uint32_t firstIP) :
                sat_(satBase),
                prcb_(prcbPointer),
                check_(checkWord),
                firstIP_(firstIP),
                cs1_(computeCS1(sat_, prcb_, firstIP)),
                reserved_{0, 0, 0xFFFF'FFFF} {

        }
        constexpr auto getSAT() const noexcept { return sat_; }
        constexpr auto getPRCB() const noexcept { return prcb_; }
        constexpr auto getCheckWord() const noexcept { return check_; }
        constexpr auto getFirstIP() const noexcept { return firstIP_; }
        constexpr auto getCS1() const noexcept { return cs1_; }
        constexpr auto getReserved(int index) const noexcept { return reserved_[index % 3]; }
        uint32_t sat_;
        uint32_t prcb_;
        uint32_t check_;
        uint32_t firstIP_;
        uint32_t cs1_;
        uint32_t reserved_[3];
    };

    constexpr CoreInitializationBlock cib(0, 0xb0, 0, 0x6ec);
    static_assert(cib.getCS1() == 0xffff'f864, "Incorrect PRCB check word value computation");
    uint16_t readBootStructures(SplitWord32 address);
    void writeBootStructures(SplitWord32 address, uint16_t value, EnableStyle style);
    void begin() noexcept;
}
#endif //CHIPSET_BOOT960_H
