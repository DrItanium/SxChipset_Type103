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
Y(Info_GetExternalIAC, 0x002)
// serial operations begin
Y(Serial_RW, 0x010)
Y(Serial_Flush, 0x011)
Y(Serial_Baud, 0x012)
// timer operations begin
Y(Timer_SystemTimer_CompareValue,0x020)
Y(Timer_SystemTimer_Prescalar, 0x021)
Y(Timer_Unixtime, 0x022)
#if 0
// Display Operations begin
Y(Display_RW, 0x030)
Y(Display_Flush, 0x031)
Y(Display_WidthHeight, 0x032)
Y(Display_Rotation, 0x033)
Y(Display_InvertDisplay, 0x034)
Y(Display_ScrollTo, 0x035)
Y(Display_SetScrollMargins, 0x036)
Y(Display_SetAddressWindow, 0x037)
Y(Display_CursorX, 0x038)
Y(Display_CursorY, 0x039)
Y(Display_CursorXY, 0x03a)
Y(Display_DrawPixel, 0x03b)
Y(Display_DrawFastVLine, 0x03c)
Y(Display_DrawFastHLine, 0x03d)
Y(Display_FillRect, 0x03e)
Y(Display_FillScreen, 0x03f)
Y(Display_DrawLine, 0x040)
Y(Display_DrawRect, 0x041)
Y(Display_DrawCircle, 0x042)
Y(Display_FillCircle, 0x043)
Y(Display_DrawTriangle, 0x044)
Y(Display_FillTriangle, 0x045)
Y(Display_DrawRoundRect, 0x046)
Y(Display_FillRoundRect, 0x047)
Y(Display_SetTextWrap, 0x048)
Y(Display_DrawChar_Square, 0x049)
Y(Display_DrawChar_Rectangle, 0x04a)
Y(Display_SetTextSize_Square, 0x04b)
Y(Display_SetTextSize_Rectangle, 0x04c)
Y(Display_SetTextColor0, 0x04d)
Y(Display_SetTextColor1, 0x04e)
Y(Display_StartWrite, 0x04f)
Y(Display_WritePixel, 0x050)
Y(Display_WriteFillRect, 0x051)
Y(Display_WriteFastVLine, 0x052)
Y(Display_WriteFastHLine, 0x053)
Y(Display_WriteLine, 0x054)
Y(Display_EndWrite, 0x055)
// start at 0x100 here
#define X(index) Y(Display_ReadCommand8_ ## index, ((index + 0x100)))
#include "Entry255.def"
#undef X
#endif
#undef Y
};
constexpr uint8_t getIOOpcode_Group(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 8);
}
constexpr uint8_t getIOOpcode_Offset(IOOpcodes opcode) noexcept {
    return static_cast<uint8_t>(static_cast<uint16_t>(opcode) >> 4);
}
#endif // end defined CHIPSET_IOOPCODES_H__
