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
    constexpr uint32_t SystemAddressTableBase = 0x1000;
    constexpr uint32_t PRCBBase = 0x2000;
    constexpr uint32_t SystemProcedureTableBase = 0x3000;
    constexpr uint32_t FaultProcedureTableBase = 0x4000;
    constexpr uint32_t InterruptTableAddress = 0x5000;
    constexpr uint32_t StartIPBase = 0x6000;
    constexpr uint32_t InterruptTableRAM = 0x1'0000;
    constexpr uint32_t InterruptStackBaseAddress = 0x2'0000;
    constexpr uint32_t UserStackBaseAddress = 0x3'0000;
    constexpr uint32_t SupervisorStackBaseAddress = 0x4'0000;
    [[gnu::used]] constexpr PROGMEM1 i960::InitialBootRecord ibr {SystemAddressTableBase, PRCBBase, 0, StartIPBase};
    [[gnu::used]] constexpr PROGMEM1 i960::SystemAddressTable sat {
        i960::SegmentDescriptor{}, // 0
        i960::SegmentDescriptor{}, // 1
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
}

void
i960::begin() noexcept {
}