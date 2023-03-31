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

#ifndef SXCHIPSET_TYPE103_PERIPHERAL_H
#define SXCHIPSET_TYPE103_PERIPHERAL_H
#include <Arduino.h>
#include "Detect.h"
#include "Types.h"
#include "MCP23S17.h"
#include "Pinout.h"
#include "Setup.h"


enum class TargetPeripheral {
    Info, 
    Serial,
    Timer,
    Count,
};
static_assert(static_cast<byte>(TargetPeripheral::Count) <= 256, "Too many Peripheral devices!");

template<typename E>
constexpr bool validOperation(E value) noexcept {
    return static_cast<int>(value) >= 0 && (static_cast<int>(value) < static_cast<int>(E::Count));
}
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    Platform::signalReady();
}
/**
 * @brief An opcode combined with arguments that we pass to Peripherals to parse
 */
struct Instruction {
    SplitWord32 opcode_;
    // there are up to four 32-bit words so we need to stash information
    // important to this here, the byte enable bits should _not_ be included
    SplitWord128 args_; // a single transaction is up to 16-bytes or four 32-bit
                        // words in size
    [[nodiscard]] constexpr auto getGroup() const noexcept { return opcode_.getIOGroup(); }
    [[nodiscard]] constexpr auto getDevice() const noexcept { return opcode_.getIODevice<TargetPeripheral>(); }
    [[nodiscard]] constexpr auto getMinor() const noexcept { return opcode_.getIOMinorCode(); }
    template<typename T>
    [[nodiscard]] constexpr auto getFunction() const noexcept { return opcode_.getIOFunction<T>(); }
    [[nodiscard]] SplitWord32& operator[](int index) noexcept { return args_[index]; }
    [[nodiscard]] volatile SplitWord32& operator[](int index) volatile noexcept { return args_[index]; }
    [[nodiscard]] const SplitWord32& operator[](int index) const noexcept { return args_[index]; }
};

template<typename E, typename T>
class OperatorPeripheral {
public:
    using OperationList = E;
    using Child = T;
    //~OperatorPeripheral() override = default;
    bool begin() noexcept { return static_cast<Child*>(this)->init(); }
    [[nodiscard]] bool available() const noexcept { return static_cast<const Child*>(this)->isAvailable(); }
    [[nodiscard]] constexpr uint8_t size() const noexcept { return size_; }
    void performRead(const SplitWord32 opcode, SplitWord128& instruction) const noexcept {
        auto theOpcode = opcode.getIOFunction<E>();
        switch (theOpcode) {
            case E::Available:
                instruction.bytes[0] = available();
                break;
            case E::Size:
                instruction.bytes[0] = size();
                break;
            default:
                if (validOperation(theOpcode)) {
                    static_cast<const Child*>(this)->extendedRead(theOpcode, opcode, instruction);
                }
                break;
        }
    }
    void performWrite(const SplitWord32 opcode, const SplitWord128& instruction) noexcept {
        auto theOpcode = opcode.getIOFunction<E>();
        switch (theOpcode) {
            case E::Available:
            case E::Size:
                // do nothing
                break;
            default:
                if (validOperation(theOpcode)) {
                    static_cast<Child*>(this)->extendedWrite(theOpcode, opcode, instruction);
                }
                break;
        }
    }
private:
    uint8_t size_{static_cast<uint8_t>(E::Count) };
};

#define BeginDeviceOperationsList(name) enum class name ## Operations : byte { Available, Size,
#define EndDeviceOperationsList(name) Count, }; static_assert(static_cast<byte>( name ## Operations :: Count ) <= 256, "Too many " #name "Operations entries defined!");
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
