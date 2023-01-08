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
    const i960::CoreInitializationBlock cib PROGMEM1 = i960::CoreInitializationBlock(0, 0xb0, 0, 0x6ec);
}

void
i960::begin() noexcept {
    uint8_t bytes[sizeof(i960::CoreInitializationBlock)];
    for (size_t i = 0; i < sizeof(cib); ++i) {
        bytes[i] = pgm_read_byte_far(pgm_get_far_address(cib) + i);
    }
    auto* ptr = reinterpret_cast<i960::CoreInitializationBlock*>(bytes);
    Serial.print(F("SAT Address: 0x")) ;
    Serial.println(ptr->getSAT(), HEX);
}