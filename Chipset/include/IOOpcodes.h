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

#ifndef CHIPSET_IOOPCODES_H__
#define CHIPSET_IOOPCODES_H__
#include <stdint.h>
enum class IOOpcodes : uint16_t {
#define Y(name, opcode) name = (static_cast<uint16_t>(opcode) << 4),
// info opcodes
Y(Info_GetCPUClockSpeed, 0x000)
Y(Info_GetChipsetClockSpeed, 0x001)
// serial operations begin
Y(Serial_RW, 0x010)
Y(Serial_Flush, 0x011)
// timer operations begin
Y(SystemTimer_TCCR1A, 0x020)
Y(SystemTimer_TCCR1B, 0x021)
Y(SystemTimer_TCCR1C, 0x022)
Y(SystemTimer_TCNT1, 0x023)
Y(SystemTimer_ICR1, 0x024)
Y(SystemTimer_OCR1A, 0x025)
Y(SystemTimer_OCR1B, 0x026)
Y(SystemTimer_OCR1C, 0x027)

Y(SystemTimer_TCCR3A, 0x028)
Y(SystemTimer_TCCR3B, 0x029)
Y(SystemTimer_TCCR3C, 0x02a)
Y(SystemTimer_TCNT3, 0x02b)
Y(SystemTimer_ICR3, 0x02c)
Y(SystemTimer_OCR3A, 0x02d)
Y(SystemTimer_OCR3B, 0x02e)
Y(SystemTimer_OCR3C, 0x02f)

Y(SystemTimer_TCCR4A, 0x030)
Y(SystemTimer_TCCR4B, 0x031)
Y(SystemTimer_TCCR4C, 0x032)
Y(SystemTimer_TCNT4, 0x033)
Y(SystemTimer_ICR4, 0x034)
Y(SystemTimer_OCR4A, 0x035)
Y(SystemTimer_OCR4B, 0x036)
Y(SystemTimer_OCR4C, 0x037)

Y(SystemTimer_TCCR5A, 0x038)
Y(SystemTimer_TCCR5B, 0x039)
Y(SystemTimer_TCCR5C, 0x03a)
Y(SystemTimer_TCNT5, 0x03b)
Y(SystemTimer_ICR5, 0x03c)
Y(SystemTimer_OCR5A, 0x03d)
Y(SystemTimer_OCR5B, 0x03e)
Y(SystemTimer_OCR5C, 0x03f)


// AVR register exposure begin
// 0x1000:0x1fff is the dual ported ram
#define X(index) Y(DualPortedRAM_Slice_0x ## index, (0x100 + index))
#include "Entry255.def"
#undef X
#undef Y
};
constexpr uint8_t getIOOpcode_Group(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 12);
}
constexpr uint8_t getIOOpcode_Offset(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 4);
}
#endif // end defined CHIPSET_IOOPCODES_H__
