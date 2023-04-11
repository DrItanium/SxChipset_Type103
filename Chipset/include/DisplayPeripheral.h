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

#ifndef CHIPSET_DISPLAY_PERIPHERAL_H__
#define CHIPSET_DISPLAY_PERIPHERAL_H__
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "Types.h"
#include "Detect.h"
#include "Peripheral.h"

BeginDeviceOperationsList(DisplayDevice)
    RW, // for the "serial" aspect so we can print out to the screen
    Flush,
    DisplayWidthHeight,
    Rotation,
    InvertDisplay,
    ScrollTo,
    SetScrollMargins,
    SetAddressWindow,
    ReadCommand8,
    CursorX,
    CursorY,
    CursorXY,
    DrawPixel,
    DrawFastVLine,
    DrawFastHLine,
    FillRect,
    FillScreen,
    DrawLine,
    DrawRect,
    DrawCircle,
    FillCircle,
    DrawTriangle,
    FillTriangle,
    DrawRoundRect,
    FillRoundRect,
    SetTextWrap,
    DrawChar_Square,
    DrawChar_Rectangle,
    SetTextSize_Square,
    SetTextSize_Rectangle,
    SetTextColor0,
    SetTextColor1,
    // Transaction parts
    StartWrite,
    WritePixel,
    WriteFillRect,
    WriteFastVLine,
    WriteFastHLine,
    WriteLine,
    EndWrite,
    /// @todo add drawBitmap support

EndDeviceOperationsList(DisplayDevice)

ConnectPeripheral(TargetPeripheral::Display, DisplayDeviceOperations);

class DisplayInterface {
    public:
        void begin() noexcept;
        [[nodiscard]] bool isAvailable() const noexcept { return true; }
        void handleWriteOperations(const SplitWord128& result, uint8_t function, uint8_t offset) noexcept;
        void handleReadOperations(SplitWord128& result, uint8_t function, uint8_t offset) noexcept;
    private:
        Adafruit_ILI9341 tft_{
                static_cast<uint8_t>(Pin::TFTCS),
                static_cast<uint8_t>(Pin::TFTDC)};
};
#endif // CHIPSET_DISPLAY_PERIPHERAL_H__
