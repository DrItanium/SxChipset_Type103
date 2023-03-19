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

[[gnu::always_inline]]
inline void
halfCycleDelay() noexcept {
    asm volatile ("nop");
}

[[gnu::always_inline]]
inline 
volatile ProcessorInterface&
getProcessorInterface() noexcept {
    return adjustedMemory<ProcessorInterface>(0);
}
enum class CPUKind : uint8_t  {
    Sx = 0b000,
};

enum class ByteEnableKind : uint8_t {
    Full32 = 0b0000,
    Upper24 = 0b0001,
    Upper16_Lowest8 = 0b0010,
    Upper16 = 0b0011,
    Highest8_Lower16 = 0b0100,
    Highest8_Lower8 = 0b0101,
    Highest8_Lowest8 = 0b0110,
    Highest8 = 0b0111,
    Lower24 = 0b1000,
    Mid16 = 0b1001,
    Higher8_Lowest8 = 0b1010,
    Higher8 = 0b1011,
    Lower16 = 0b1100,
    Lower8 = 0b1101,
    Lowest8 = 0b1110,
    Nothing = 0b1111,
};
/**
 * @brief Generated as part of write operations
 */
struct WritePacket {
    uint32_t address;
    ByteEnableKind mask;
    uint32_t value;
};

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
        static void doReset(decltype(LOW) value) noexcept;
        static void doHold(decltype(LOW) value) noexcept;
        static uint8_t getCPUConfigValue() noexcept;
        static uint8_t getFrequencyInfo() noexcept;
        static void tellCPUToBackOff() noexcept;
        static void signalNMI() noexcept;
        static void signalXINT0() noexcept;
        static void signalXINT1() noexcept;
        static void signalXINT2() noexcept;
        static void signalXINT3() noexcept;
        static void signalXINT4() noexcept;
        static void signalXINT5() noexcept;
        static void signalXINT6() noexcept;
        static void signalXINT7() noexcept;
        static void toggleXINT0() noexcept;
        static void toggleXINT1() noexcept;
        static void toggleXINT2() noexcept;
        static void toggleXINT3() noexcept;
        static void toggleXINT4() noexcept;
        static void toggleXINT5() noexcept;
        static void toggleXINT6() noexcept;
        static void toggleXINT7() noexcept;
        static uint32_t getDataLines() noexcept;
        static void setDataLines(uint32_t value) noexcept;
        static void waitForDataState() noexcept;
        static void setBankConfiguration(bool value) noexcept;
        static uint32_t readAddress() noexcept;
        static void signalReady() noexcept;
        static bool checksumFailure() noexcept;
        static bool isBurstLast() noexcept;
        static uint8_t getByteEnable() noexcept;
        static bool isReadOperation() noexcept;
        static void configureDataLinesForWrite() noexcept;
        static void configureDataLinesForRead() noexcept;
        static bool isIOOperation() noexcept;
        static inline CPUKind getInstalledCPUKind() noexcept { return static_cast<CPUKind>(getProcessorInterface().control_.ctl.cfg); }
    private:
        static inline bool initialized_ = false;
};

#endif // end SXCHIPSET_TYPE103_SETUP_H__
