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
Y(Serial_Baud, 0x012)
// timer operations begin
Y(Timer_SystemTimer_CompareValue,0x020)
Y(Timer_SystemTimer_Prescalar, 0x021)
Y(Timer_Unixtime, 0x022)
// 0x1000:0x1fff is the dual ported ram
#undef Y
};
constexpr uint8_t getIOOpcode_Group(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 8);
}
constexpr uint8_t getIOOpcode_Offset(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 4);
}
#endif // end defined CHIPSET_IOOPCODES_H__
