/*
i960SxChipset_Type103
Copyright (c) 2022, Joshua Scoggins
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

#ifndef SXCHIPSET_TYPE103_SETUP_H__
#define SXCHIPSET_TYPE103_SETUP_H__
#include <Arduino.h>
#include "Detect.h"
#include "Types.h"

constexpr bool EnableDebugMode = false;

[[gnu::always_inline]]
inline void
singleCycleDelay() noexcept {
    asm volatile ("nop");
    asm volatile ("nop");
}


class Platform final {
    public:
        Platform() = delete;
        ~Platform() = delete;
        Platform(const Platform&) = delete;
        Platform(Platform&&) = delete;
        Platform& operator=(const Platform&) = delete;
        Platform& operator=(Platform&&) = delete;
    public:
        static void begin() noexcept;
        static void startAddressTransaction() noexcept;
        static void collectAddress() noexcept;
        static void endAddressTransaction() noexcept;
        static void doReset(decltype(LOW) value) noexcept;
        static void doHold(decltype(LOW) value) noexcept;
        static uint16_t getDataLines() noexcept;
        static void setDataLines(uint16_t) noexcept;
        static bool isReadOperation() noexcept { return isReadOperation_; }
        static SplitWord32 getAddress() noexcept { return address_; }
    private:
        static inline bool isReadOperation_ = false;
        static inline SplitWord32 address_{0};
        static inline bool initialized_ = false;
        static inline uint8_t dataLinesDirection_ = 0;
        static inline SplitWord16 previousValue_{0};
};

#endif // end SXCHIPSET_TYPE103_SETUP_H__
