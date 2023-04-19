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
#include "Pinout.h"

constexpr bool MCUHasDirectAccess = true;
constexpr bool XINT0DirectConnect = true;
constexpr bool XINT1DirectConnect = false;
constexpr bool XINT2DirectConnect = false;
constexpr bool XINT3DirectConnect = false;
constexpr bool XINT4DirectConnect = false;
constexpr bool XINT5DirectConnect = false;
constexpr bool XINT6DirectConnect = false;
constexpr bool XINT7DirectConnect = false;
constexpr bool DirectlyConnectedD0_7 = true;
constexpr bool DirectlyConnectedD8_15 = false;
constexpr bool DirectlyConnectedD16_23 = false;
constexpr bool DirectlyConnectedD24_31 = false;

template<uint8_t count>
[[gnu::always_inline]]
inline void
insertCustomNopCount() noexcept {
    __builtin_avr_nops(count);
}
[[gnu::always_inline]]
inline void
singleCycleDelay() noexcept {
    insertCustomNopCount<2>();
}

[[gnu::always_inline]]
inline void
halfCycleDelay() noexcept {
    insertCustomNopCount<1>();
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
    Generic,
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
constexpr bool isUpper16bitValue(uint32_t address) noexcept {
    return (address & 0b11) == 0b10;
}
constexpr bool isLower16bitValue(uint32_t address) noexcept {
    return alignedTo32bitBoundaries(address);
}
static_assert(alignedTo32bitBoundaries(0), "32-bit alignment logic is broken");
static_assert(!alignedTo32bitBoundaries(0b10), "32-bit alignment logic is broken");
static_assert(isUpper16bitValue(0b10), "is 16-bit upper half check is broken");
static_assert(!isUpper16bitValue(0), "is 16-bit upper half check is broken");
static_assert(isLower16bitValue(0), "is 16-bit lower half check is broken");
static_assert(!isLower16bitValue(0b10), "is 16-bit lower half check is broken");

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
        [[gnu::always_inline]] inline static void waitForDataState() noexcept { while (digitalRead<Pin::DEN>() == HIGH); }
        static void setBankConfiguration(bool value) noexcept;
        [[gnu::always_inline]] inline static void signalReady() noexcept { toggle<Pin::READY>(); }
        static bool checksumFailure() noexcept;
        [[gnu::always_inline]] inline static bool isBurstLast() noexcept { return digitalRead<Pin::BLAST>() == LOW; }
        [[gnu::always_inline]] inline static uint8_t getByteEnable() noexcept { return getInputRegister<Port::SignalCTL>(); }
        [[gnu::always_inline]] inline static bool isWriteOperation() noexcept { return digitalRead<Pin::WR>(); }
        static inline CPUKind getInstalledCPUKind() noexcept { return static_cast<CPUKind>(ControlSignals.ctl.cfg); }
        static void setBank(const SplitWord32& addr, AccessFromIBUS) noexcept;
        static void setBank(const SplitWord32& addr, AccessFromXBUS) noexcept;
        static void setBank(uint8_t bankId, AccessFromIBUS) noexcept;
        static void setBank(uint32_t bankAddress, AccessFromXBUS) noexcept;
        static uint8_t getBank(AccessFromIBUS) noexcept;
        static uint32_t getBank(AccessFromXBUS) noexcept;
        /**
         * @brief Return a pointer to the closest aligned SplitWord32 on the IBUS
         */
        static volatile SplitWord32& getMemoryView(const SplitWord32& addr, AccessFromIBUS) noexcept;
        /**
         * @brief Return a pointer to the closest aligned SplitWord32 on the XBUS
         */
        static volatile SplitWord32& getMemoryView(const SplitWord32& addr, AccessFromXBUS) noexcept;
        static volatile SplitWord128& getTransactionWindow(const SplitWord32& addr, AccessFromIBUS) noexcept;
        static volatile SplitWord128& getTransactionWindow(const SplitWord32& addr, AccessFromXBUS) noexcept;
        static volatile SplitWord128& getTransactionWindow(uint32_t addr, AccessFromXBUS) noexcept;
        static volatile uint8_t* viewAreaAsBytes(const SplitWord32& addr, AccessFromIBUS) noexcept;
        static volatile uint8_t* viewAreaAsBytes(const SplitWord32& addr, AccessFromXBUS) noexcept;
};

#endif // end SXCHIPSET_TYPE103_SETUP_H__
