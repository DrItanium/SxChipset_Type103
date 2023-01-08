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
    constexpr uint32_t l0Program[] PROGMEM1{
        0,
        0xb0,
        0,
        0x6ec,
        i960::computeCS1(0, 0xb0, 0x6ec),
        0,
        0,
        0xFFFF'FFFF,
    };
    constexpr size_t l0ProgramSize PROGMEM1 = sizeof(l0Program);
}

void
i960::begin() noexcept {
    size_t l0ByteCount = pgm_read_word_far(pgm_get_far_address(l0ProgramSize));
    size_t l0WordCount = l0ByteCount / sizeof(uint32_t);
    Serial.print(F("L0 Program Size (bytes): 0x"));
    Serial.println(l0ByteCount, HEX);
    Serial.print(F("L0 Program Size (32-bit words): 0x"));
    Serial.println(l0WordCount, HEX);
    Serial.println(F("Boot ROM Words: "));
    for (size_t i = 0; i < l0WordCount; ++i) {
        Serial.print(i);
        Serial.print(F(": 0x"));
        Serial.println(pgm_read_dword_far(pgm_get_far_address(l0Program) + (i * sizeof(uint32_t))), HEX);
    }
}