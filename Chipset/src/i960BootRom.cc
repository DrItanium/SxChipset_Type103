/*
i960SxChipset_Type103
Copyright (c) 2023, Joshua Scoggins
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
#include "Types.h"
#include "Boot960.h"

namespace
{
    template<typename T>
    constexpr uint32_t computeNextBaseAddress_SALIGN(uint32_t baseAddress) noexcept {
        return i960::alignToSALIGNBoundaries(baseAddress + sizeof(T));
    }
    template<typename T>
    constexpr uint32_t computeNextBaseAddress_Align8(uint32_t baseAddress) noexcept {
        return i960::alignTo256ByteBoundaries(baseAddress + sizeof(T));
    }
    constexpr uint32_t AddressTable[] {
        0,
    };
    constexpr uint32_t SystemAddressTableBase = 0;
    // PRCB must be aligned to 64byte boundaries for the Sx processor
    constexpr uint32_t PRCBBase = computeNextBaseAddress_SALIGN<i960::SystemAddressTable>(SystemAddressTableBase);
    constexpr uint32_t SystemProcedureTableBase = computeNextBaseAddress_SALIGN<i960::PRCB>(PRCBBase);
    constexpr uint32_t FaultProcedureTableBase = computeNextBaseAddress_SALIGN<i960::SystemProcedureTable>(SystemProcedureTableBase);
    constexpr uint32_t FaultTableBase = computeNextBaseAddress_Align8<i960::SystemProcedureTable>(FaultProcedureTableBase);
    constexpr uint32_t InterruptTableAddress = computeNextBaseAddress_SALIGN<i960::FaultTable>(FaultTableBase);
    constexpr uint32_t StartIPBase = computeNextBaseAddress_Align8<i960::InterruptTable>(InterruptTableAddress);
    constexpr uint32_t RAMStartAddress = i960::alignToSALIGNBoundaries(2l * 1024l * 1024l);
    constexpr uint32_t InterruptTableCopyStart = RAMStartAddress; // start at the beginning of RAM
    constexpr uint32_t InterruptStackStartPosition = computeNextBaseAddress_SALIGN<>()
            i960::alignToSALIGNBoundaries(2l*1024l*1024l); // start at the beginning of RAM
    // put the SAT and IBR together to save on alignment and space
    [[gnu::used]] constexpr PROGMEM1 i960::SystemAddressTable sat {
        i960::SegmentDescriptor{SystemAddressTableBase, PRCBBase, 0, StartIPBase }, // 0
        i960::SegmentDescriptor{i960::computeCS1(SystemAddressTableBase, PRCBBase, StartIPBase), 0, 0, 0xffff'ffff}, // 1
        i960::SegmentDescriptor{}, // 2
        i960::SegmentDescriptor{}, // 3
        i960::SegmentDescriptor{}, // 4
        i960::SegmentDescriptor{}, // 5
        i960::SegmentDescriptor{}, // 6
        i960::SegmentDescriptor{SystemProcedureTableBase, 0x304000fb}, // 7
        i960::SegmentDescriptor{SystemAddressTableBase, 0x00fc00fb}, // 8
        i960::SegmentDescriptor{SystemProcedureTableBase, 0x304000fb}, // 9
        i960::SegmentDescriptor{FaultProcedureTableBase, 0x304000fb}, // 10
    };
    [[gnu::used]] constexpr PROGMEM1 i960::PRCB prcb {
            0,
            0xc,
            { 0 },
            InterruptTableAddress,


    };
}

void
i960::begin() noexcept {
}