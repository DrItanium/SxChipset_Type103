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
/**
 * @brief The CPU Module kind as described in the Cyclone SDK manual
 */
enum class CPUKind : uint8_t  {
    /// @brief The CPU installed is a 80960Sx processor
    Sx = 0b000,
    /// @brief The CPU installed is a 80960Kx processor
    Kx = 0b001,
    /// @brief The CPU installed is a 80960Cx processor
    Cx = 0b010,
    /// @brief The CPU installed is a 80960Hx processor
    Hx = 0b011,
    /// @brief The CPU installed is a 80960Jx processor
    Jx = 0b100,
    Reserved0 = 0b101,
    Reserved1 = 0b110,
    Reserved2 = 0b111,
};

enum class NativeBusWidth : uint8_t {
    Unknown,
    Sixteen,
    ThirtyTwo,
};

constexpr NativeBusWidth getBusWidth(CPUKind kind) noexcept {
    switch (kind) {
        case CPUKind::Sx:
            return NativeBusWidth::Sixteen;
        case CPUKind::Kx:
        case CPUKind::Cx:
        case CPUKind::Hx:
        case CPUKind::Jx:
            return NativeBusWidth::ThirtyTwo;
        default:
            return NativeBusWidth::Unknown;
    }
}

/// @brief The Cyclone SDK board supported many different clock speeds, we only support a 10MHz synchronized with the AVR chip
enum class CPUFrequencyInfo {
    /// @brief The only supported frequency
    Clock10MHz = 0b000,
};

/// @brief the bytes to operate on in a single memory transaction cycle
enum class ByteEnableKind : uint8_t {
    /// @brief A full 32-bit value
    Full32 = 0b0000,
    /// @brief Operate on the upper 16-bits only
    Upper16 = 0b0011,
    /// @brief operate on the lower 16-bits only
    Lower16 = 0b1100,
    /// @brief only the highest 8-bits
    Highest8 = 0b0111,
    /// @brief bits 16-23
    Higher8 = 0b1011,
    /// @brief bits 8-15
    Lower8 = 0b1101,
    /// @brief bits 0-7
    Lowest8 = 0b1110,

    // misalignment states
    /// @brief The upper 24-bits of the 32-bit value, generally implies misalignment
    Upper24 = 0b0001,
    /// @brief Operate on the lower 24-bits, generally implies a misalignment
    Lower24 = 0b1000,
    /// @brief a misaligned 16-bit operation
    Mid16 = 0b1001,


    // Impossible states (I believe)
    Nothing = 0b1111,
    Highest8_Lower16 = 0b0100,
    Highest8_Lower8 = 0b0101,
    Highest8_Lowest8 = 0b0110,
    Upper16_Lowest8 = 0b0010,
    Higher8_Lowest8 = 0b1010,
};
/**
 * @brief Given a 32-bit bus, is the given kind a legal one; A false here means
 * that there are holes in byte enable pattern; I do not think that is possible
 * with how the i960 operates
 */
constexpr bool isLegalState(ByteEnableKind kind) noexcept {
    switch (kind) {
        case ByteEnableKind::Full32:
        case ByteEnableKind::Upper24:
        case ByteEnableKind::Upper16:
        case ByteEnableKind::Highest8:
        case ByteEnableKind::Lower24:
        case ByteEnableKind::Mid16:
        case ByteEnableKind::Higher8:
        case ByteEnableKind::Lower16:
        case ByteEnableKind::Lower8:
        case ByteEnableKind::Lowest8:
            return true;
        default:
            return false;
    }
}

constexpr bool isPossibleState(NativeBusWidth width, ByteEnableKind kind) noexcept {
    switch (width) {
        case NativeBusWidth::Sixteen: 
            {
                switch (kind) {
                    case ByteEnableKind::Upper16:
                    case ByteEnableKind::Lower16:
                    case ByteEnableKind::Highest8:
                    case ByteEnableKind::Higher8:
                    case ByteEnableKind::Lower8:
                    case ByteEnableKind::Lowest8:
                        return true;
                    default:
                        return false;
                }
            }
        case NativeBusWidth::ThirtyTwo:
            return isLegalState(kind);
        default:
            return false;
    }
}

constexpr bool isMisaligned(ByteEnableKind kind) noexcept {
    switch (kind) {
        case ByteEnableKind::Full32:
        case ByteEnableKind::Upper16:
        case ByteEnableKind::Lower16:
        case ByteEnableKind::Highest8:
        case ByteEnableKind::Higher8:
        case ByteEnableKind::Lower8:
        case ByteEnableKind::Lowest8:
            return false;
        default:
            return true;
    }
}
template<ByteEnableKind kind>
constexpr auto LegalState_v = isLegalState(kind);

template<ByteEnableKind kind>
constexpr auto Misaligned_v = isMisaligned(kind);

constexpr bool alignedTo32bitBoundaries(uint32_t address) noexcept {
    return (address & 0b11) == 0;
}
static_assert(alignedTo32bitBoundaries(0b11111100), "32-bit alignment logic is broken");
static_assert(!alignedTo32bitBoundaries(0b11111110), "32-bit alignment logic is broken");
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
