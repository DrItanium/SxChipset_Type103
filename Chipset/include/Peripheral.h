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
#include "RTClib.h"


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
template<typename T, bool introduceCpuCycleDelay = false>
inline T
readInputChannelAs() noexcept {
    // make sure there is a builtin delay
    if constexpr (introduceCpuCycleDelay) {
        asm volatile ("nop");
        asm volatile ("nop");
    }
    return T{readFromCapture()};
}
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    pulse<Pin::Ready, LOW, HIGH>();
}

extern SplitWord16 previousValue;
extern uint16_t currentDataLinesValue;

template<typename T>
class DynamicValue : public OperationHandler<DynamicValue<T>> {
    public:
        static_assert(sizeof(T) <= 16, "Type is larger than 16-bytes! Use a different class!");
        DynamicValue(T value) noexcept : words_{} {
            // have to set it up this way
            value_ = value;
        }
        uint16_t performRead(const Channel0Value& m0) const noexcept {
            return words_[this->getOffset()].getWholeValue();
        }
        void performWrite(const Channel0Value& m0, uint16_t value) noexcept {
            SplitWord16 tmp(value);
            switch (m0.getByteEnable()) {
                case EnableStyle::Full16:
                    words_[this->getOffset()] = tmp;
                    break;
                case EnableStyle::Lower8:
                    words_[this->getOffset()].bytes[0] = tmp.bytes[0];
                    break;
                case EnableStyle::Upper8:
                    words_[this->getOffset()].bytes[1] = tmp.bytes[1];
                    break;
                default:
                    break;
            }
        }
        [[nodiscard]] T getValue() const noexcept { return value_; }
    private:
        union {
            T value_;
            SplitWord16 words_[16 / sizeof(SplitWord16)]; // make sure that we
                                                          // can never overflow
        };
};

template<>
class DynamicValue<uint32_t> : public OperationHandler<DynamicValue<uint32_t>> {
    public:
        DynamicValue(uint32_t value) noexcept : value_{value} { }
        uint16_t performRead(const Channel0Value& m0) const noexcept {
            return value_.halves[getOffset() & 0b1]; 
        }
        void performWrite(const Channel0Value& m0, uint16_t value) noexcept {
            SplitWord16 tmp(value);
            switch (m0.getByteEnable()) {
                case EnableStyle::Full16:
                    value_.word16[getOffset() & 0b1] = tmp;
                    break;
                case EnableStyle::Lower8:
                    value_.word16[getOffset() & 0b1].bytes[0] = tmp.bytes[0];
                    break;
                case EnableStyle::Upper8:
                    value_.word16[getOffset() & 0b1].bytes[1] = tmp.bytes[1];
                    break;
                default:
                    break;
            }
        }
        [[nodiscard]] uint32_t getValue() const noexcept { return value_.getWholeValue(); }
    private:
        SplitWord32 value_;
};

template<>
class DynamicValue<uint16_t> : public OperationHandler<DynamicValue<uint16_t>> {
    public:
        DynamicValue(uint16_t value) noexcept : value_{value} { }
        uint16_t performRead(const Channel0Value& m0) const noexcept {
            return value_.getWholeValue();
        }
        void performWrite(const Channel0Value& m0, uint16_t value) noexcept {
            SplitWord16 tmp(value);
            switch (m0.getByteEnable()) {
                case EnableStyle::Full16:
                    value_.full = value;
                    break;
                case EnableStyle::Lower8:
                    value_.bytes[0] = tmp.bytes[0];
                    break;
                case EnableStyle::Upper8:
                    value_.bytes[1] = tmp.bytes[1];
                    break;
                default:
                    break;
            }
        }
        [[nodiscard]] uint16_t getValue() const noexcept { return value_.getWholeValue(); }
    private:
        SplitWord16 value_;
};

using ExpressUint16_t = DynamicValue<uint16_t>;
using ExpressUint32_t = DynamicValue<uint32_t>;
using ExpressUint64_t = DynamicValue<uint64_t>;

template<uint32_t value>
uint16_t
expose32BitConstant(const SplitWord32&, const Channel0Value&, byte offset) noexcept {
    switch (offset) {
        case 0:
            return static_cast<uint16_t>(value);
        case 1:
            return static_cast<uint16_t>(value >> 16);
        default:
            return 0;
    }
}
template<uint64_t value>
uint16_t
expose64BitConstant(const SplitWord32&, const Channel0Value&, byte offset) noexcept {
    switch (offset) {
        case 0:
            return static_cast<uint16_t>(value);
        case 1:
            return static_cast<uint16_t>(value >> 16);
        case 2:
            return static_cast<uint16_t>(value >> 32);
        case 3:
            return static_cast<uint16_t>(value >> 48);
        default:
            return 0;
    }
}
template<bool value> 
uint16_t
exposeBooleanValue(const SplitWord32&, const Channel0Value&, byte) noexcept {
    return value ? 0xFFFF : 0;
}
template<typename E, typename T>
class OperatorPeripheral : public AddressTracker {
public:
    using OperationList = E;
    using Child = T;
    //~OperatorPeripheral() override = default;
    bool begin() noexcept { return static_cast<Child*>(this)->init(); }
    bool available() const noexcept { return static_cast<const Child*>(this)->isAvailable(); }
    uint32_t size() const noexcept { return size_.getWholeValue(); }
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
    void endTransaction() noexcept {
        resetOpcode();
        static_cast<T*>(this)->onEndTransaction();
    }
    uint16_t performRead(const Channel0Value& m0) const noexcept {
        switch (currentOpcode_) {
            case E::Available:
                return available() ? 0xFFFF : 0x0000;
            case E::Size:
                return size_.retrieveHalf(getOffset());
            default:
                if (validOperation(currentOpcode_)) {
                    return static_cast<const Child*>(this)->extendedRead(m0);
                } else {
                    return 0;
                }
        }
    }
    void performWrite(const Channel0Value& m0, uint16_t value) noexcept {
        switch (currentOpcode_) {
            case E::Available:
            case E::Size:
                // do nothing
                break;
            default:
                if (validOperation(currentOpcode_)) {
                    static_cast<Child*>(this)->extendedWrite(m0, value);
                }
                break;
        }
    }
    uint16_t read(const Channel0Value& m0) const noexcept {
        return performRead(m0);
    }
    void write(const Channel0Value& m0, uint16_t value) noexcept {
        performWrite(m0, value);
    }
    void next() noexcept {
        advanceOffset();
    }
protected:
    [[nodiscard]] constexpr OperationList getCurrentOpcode() const noexcept { return currentOpcode_; }
private:
    OperationList currentOpcode_ = OperationList::Count;
    SplitWord32 size_{static_cast<uint32_t>(E::Count) };
};

#define BeginDeviceOperationsList(name) enum class name ## Operations : byte { Available, Size,
#define EndDeviceOperationsList(name) Count, }; static_assert(static_cast<byte>( name ## Operations :: Count ) <= 256, "Too many " #name "Operations entries defined!");
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
