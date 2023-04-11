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
#include "DisplayPeripheral.h"

void
DisplayInterface::begin() noexcept {
    tft_.begin();
    auto x = tft_.readcommand8(ILI9341_RDMODE);
    Serial.println(F("DISPLAY INFORMATION"));
    Serial.print(F("Display Power Mode: 0x")); Serial.println(x, HEX);
    x = tft_.readcommand8(ILI9341_RDMADCTL);
    Serial.print(F("MADCTL Mode: 0x")); Serial.println(x, HEX);
    x = tft_.readcommand8(ILI9341_RDPIXFMT);
    Serial.print(F("Pixel Format: 0x")); Serial.println(x, HEX);
    x = tft_.readcommand8(ILI9341_RDIMGFMT);
    Serial.print(F("Image Format: 0x")); Serial.println(x, HEX);
    x = tft_.readcommand8(ILI9341_RDSELFDIAG);
    Serial.print(F("Self Diagnostic: 0x")); Serial.println(x, HEX);
    tft_.fillScreen(ILI9341_BLACK);
}
void 
DisplayInterface::handleWriteOperations(const SplitWord128& body, uint8_t function, uint8_t offset) noexcept {
    using K = ConnectedOpcode_t<TargetPeripheral::Display>;
    switch (getFunctionCode<TargetPeripheral::Display>(function)) {
        case K::SetScrollMargins:
            tft_.setScrollMargins(body[0].halves[0],
                    body[0].halves[1]);
            break;
        case K::SetAddressWindow:
            tft_.setAddrWindow(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::ScrollTo:
            tft_.scrollTo(body[0].halves[0]);
            break;
        case K::InvertDisplay:
            tft_.invertDisplay(body.bytes[0] != 0);
            break;
        case K::Rotation:
            tft_.setRotation(body.bytes[0]);
            break;
        case K::RW:
            tft_.print(static_cast<uint8_t>(body.bytes[0]));
            break;
        case K::Flush:
            tft_.flush();
            break;
        case K::DrawPixel:
            tft_.drawPixel(body[0].halves[0], body[0].halves[1], body[1].halves[0]);
            break;
        case K::DrawFastHLine:
            tft_.drawFastHLine(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::DrawFastVLine:
            tft_.drawFastVLine(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::FillRect:
            tft_.fillRect( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0]);
            break;

        case K::FillScreen:
            tft_.fillScreen(body[0].halves[0]);
            break;
        case K::DrawLine:
            tft_.drawLine( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0]);
            break;
        case K::DrawRect:
            tft_.drawRect( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0]);
            break;
        case K::DrawCircle:
            tft_.drawCircle(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::FillCircle:
            tft_.fillCircle(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::DrawTriangle:
            tft_.drawTriangle( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1],
                    body[3].halves[0]);
            break;
        case K::FillTriangle:
            tft_.fillTriangle( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1],
                    body[3].halves[0]);
            break;
        case K::DrawRoundRect:
            tft_.drawRoundRect( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1]);
            break;
        case K::FillRoundRect:
            tft_.fillRoundRect( body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1]);
            break;
        case K::SetTextWrap:
            tft_.setTextWrap(body.bytes[0]);
            break;
        case K::CursorX: 
            tft_.setCursor(body[0].halves[0], tft_.getCursorY());
            break;
        case K::CursorY: 
            tft_.setCursor(tft_.getCursorX(), body[0].halves[0]);
            break;
        case K::CursorXY:
            tft_.setCursor(body[0].halves[0], body[0].halves[1]);
            break;
        case K::DrawChar_Square:
            tft_.drawChar(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1]);
            break;
        case K::DrawChar_Rectangle:
            tft_.drawChar(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0],
                    body[2].halves[1],
                    body[3].halves[0]);
            break;
        case K::SetTextSize_Square:
            tft_.setTextSize(body[0].halves[0]);
            break;
        case K::SetTextSize_Rectangle:
            tft_.setTextSize(body[0].halves[0],
                    body[0].halves[1]);
            break;
        case K::SetTextColor0:
            tft_.setTextColor(body[0].halves[0]);
            break;
        case K::SetTextColor1:
            tft_.setTextColor(body[0].halves[0], body[0].halves[1]);
            break;
        case K::StartWrite:
            tft_.startWrite();
            break;
        case K::WritePixel:
            tft_.writePixel(body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0]);
            break;
        case K::WriteFillRect:
            tft_.writeFillRect(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0]);
            break;
        case K::WriteFastVLine:
            tft_.writeFastVLine(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::WriteFastHLine:
            tft_.writeFastHLine(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1]);
            break;
        case K::WriteLine:
            tft_.writeLine(
                    body[0].halves[0],
                    body[0].halves[1],
                    body[1].halves[0],
                    body[1].halves[1],
                    body[2].halves[0]);
            break;
        case K::EndWrite:
            tft_.endWrite();
            break;
        default:
            break;
    }
}
void 
DisplayInterface::handleReadOperations(SplitWord128& body, uint8_t function, uint8_t offset) noexcept {
    using K = ConnectedOpcode_t<TargetPeripheral::Display>;
    switch (getFunctionCode<TargetPeripheral::Display>(function)) {
        case K::Available:
        case K::RW:
            body[0].full = 0xFFFF'FFFF;
            break;
        case K::Size:
            body.bytes[0] = OpcodeCount_v<TargetPeripheral::Display>;
            break;
        case K::DisplayWidthHeight:
            body[0].halves[0] = tft_.width();
            body[0].halves[1] = tft_.height();
            break;
        case K::Rotation:
            body.bytes[0] = tft_.getRotation();
            break;
        case K::ReadCommand8:
            // use the offset in the instruction to determine where to
            // place the result and what to request from the tft
            // display
            body.bytes[offset & 0b1111] = tft_.readcommand8(offset);
            break;
        case K::CursorX: 
            body[0].halves[0] = tft_.getCursorX(); 
            break;
        case K::CursorY: 
            body[0].halves[0] = tft_.getCursorY(); 
            break;
        case K::CursorXY: 
            body[0].halves[0] = tft_.getCursorX();
            body[0].halves[1] = tft_.getCursorY(); 
            break;
        case K::Flush:
            // fallthrough
            tft_.flush();
        default:
            body[0].full = 0;
            return;
    }
}
