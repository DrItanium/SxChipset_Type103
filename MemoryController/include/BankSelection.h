/**
* i960SxChipset_Type103
* Copyright (c) 2022-2023, Joshua Scoggins
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef BANK_SELECTION_H__
#define BANK_SELECTION_H__
#include <stdint.h>
#include <Arduino.h>
#include "Types.h"
constexpr auto BANK0 = 45;
constexpr auto BANK1 = 44;
constexpr auto BANK2 = 43;
constexpr auto BANK3 = 42;
constexpr auto FakeA15 = 38;
constexpr auto RealA15 = PIN_PC7;
namespace External328Bus {
    void setBank(uint8_t bank) noexcept;
    void begin() noexcept;
    void select() noexcept;
} // end namespace External328Bus
namespace InternalBus {
    void setBank(uint8_t bank) noexcept;
    void begin() noexcept;
    void select() noexcept;
} // end namespace InternalBus

#endif
