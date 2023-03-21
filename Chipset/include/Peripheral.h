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
    RTC,
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
    SplitWord32 args_[4]; // a single transaction is up to 16-bytes or four 32-bit
                          // words in size
    //explicit Instruction(SplitWord32 opcode) : opcode_(opcode) { }
};
template<typename E, typename T>
class OperatorPeripheral : public AddressTracker {
public:
    using OperationList = E;
    using Child = T;
    //~OperatorPeripheral() override = default;
    bool begin() noexcept { return static_cast<Child*>(this)->init(); }
    [[nodiscard]] bool available() const noexcept { return static_cast<const Child*>(this)->isAvailable(); }
    [[nodiscard]] constexpr uint8_t size() const noexcept { return size_; }
    void stashOpcode(const SplitWord32& addr) noexcept {
        // determine where we are looking :)
        currentOpcode_ = addr.getIOFunction<OperationList>();
    }
    void resetOpcode() noexcept {
        currentOpcode_ = OperationList::Count;
    }
    void startTransaction(const SplitWord32& addr) noexcept {
        recordAddress(addr);
        stashOpcode(addr);
        static_cast<T*>(this)->onStartTransaction(addr);
    }
    [[nodiscard]] uint32_t performRead() const noexcept {
        switch (currentOpcode_) {
            case E::Available:
                return available();
            case E::Size:
                return size();
            default:
                if (validOperation(currentOpcode_)) {
                    return static_cast<const Child*>(this)->extendedRead();
                } else {
                    return 0;
                }
        }
    }
    void performWrite(uint32_t value) noexcept {
        switch (currentOpcode_) {
            case E::Available:
            case E::Size:
                // do nothing
                break;
            default:
                if (validOperation(currentOpcode_)) {
                    static_cast<Child*>(this)->extendedWrite(value);
                }
                break;
        }
    }
    [[nodiscard]] uint32_t read() const noexcept { return performRead(); }
    void write(uint32_t value) noexcept {
        performWrite(value);
    }
    void next() noexcept {
        advanceOffset();
    }
protected:
    [[nodiscard]] constexpr OperationList getCurrentOpcode() const noexcept { return currentOpcode_; }
private:
    OperationList currentOpcode_ = OperationList::Count;
    uint8_t size_{static_cast<uint8_t>(E::Count) };
};

#define BeginDeviceOperationsList(name) enum class name ## Operations : byte { Available, Size,
#define EndDeviceOperationsList(name) Count, }; static_assert(static_cast<byte>( name ## Operations :: Count ) <= 256, "Too many " #name "Operations entries defined!");
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
