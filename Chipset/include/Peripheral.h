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
enum class InfoDeviceOperations {
    Available,
    Size,
    GetChipsetClock,
    GetCPUClock,
    Count,
};
static_assert(static_cast<byte>(InfoDeviceOperations::Count) <= 256, "Too many Peripheral devices!");
enum class SerialDeviceOperations {
    Available,
    Size,
    RW,
    Flush,
    Baud,
    Count,
};

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
class DynamicValue : public OperationHandler {
    public:
        static_assert(sizeof(T) <= 16, "Type is larger than 16-bytes! Use a different class!");
        DynamicValue(T value) noexcept : words_{} {
            // have to set it up this way
            value_ = value;
        }
        uint16_t read(const Channel0Value& m0) const noexcept override { 
            return words_[getOffset()].getWholeValue(); 
        }
        void write(const Channel0Value& m0, uint16_t value) noexcept override { 
            SplitWord16 tmp(value);
            switch (m0.getByteEnable()) {
                case EnableStyle::Full16:
                    words_[getOffset()] = tmp;
                    break;
                case EnableStyle::Lower8:
                    words_[getOffset()].bytes[0] = tmp.bytes[0];
                    break;
                case EnableStyle::Upper8:
                    words_[getOffset()].bytes[1] = tmp.bytes[1];
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
class DynamicValue<uint32_t> : public OperationHandler {
    public:
        DynamicValue(uint32_t value) noexcept : value_{value} { }
        uint16_t read(const Channel0Value& m0) const noexcept override { 
            return value_.halves[getOffset() & 0b1]; 
        }
        void write(const Channel0Value& m0, uint16_t value) noexcept override { 
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
class DynamicValue<uint16_t> : public OperationHandler {
    public:
        DynamicValue(uint16_t value) noexcept : value_{value} { }
        uint16_t read(const Channel0Value& m0) const noexcept override { 
            return value_.getWholeValue();
        }
        void write(const Channel0Value& m0, uint16_t value) noexcept override { 
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
class Peripheral : public OperationHandler {
    public:
        using Parent = OperationHandler;
        ~Peripheral() override = default;
        virtual bool begin() noexcept { return true; }
};
template<typename E>
class OperatorPeripheral : public Peripheral{
public:
    using Parent = Peripheral;
    using OperationList = E;
    ~OperatorPeripheral() override = default;
    virtual bool available() const noexcept { return true; }
    virtual uint32_t size() const noexcept { return size_.getWholeValue(); }
    void startTransaction(const SplitWord32& addr) noexcept override {
        Parent::startTransaction(addr);
        // determine where we are looking :)
        currentOpcode_ = addr.getIOFunction<OperationList>();
    }
    void endTransaction() noexcept override {
        // reset the current opcode
        currentOpcode_ = OperationList::Count;
    }
    uint16_t read(const Channel0Value& m0) const noexcept override {
        switch (currentOpcode_) {
            case E::Available: 
                return available() ? 0xFFFF : 0x0000;
            case E::Size:
                return size_.retrieveWord(getOffset());
            default:
                if (validOperation(currentOpcode_)) {
                    return extendedRead(m0);
                } else {
                    return 0;
                }
        }
    }
    void write(const Channel0Value& m0, uint16_t value) noexcept override {
        switch (currentOpcode_) {
            case E::Available:
            case E::Size:
                // do nothing
                break;
            default:
                if (validOperation(currentOpcode_)) {
                    extendedWrite(m0, value);
                }
                break;
        }
    }
protected:
    //virtual void handleExtendedOperation(const SplitWord32& addr, OperationList value, OperationHandlerUser fn) noexcept = 0;
    virtual uint16_t extendedRead(const Channel0Value& m0) const noexcept = 0;
    virtual void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept = 0;
    [[nodiscard]] constexpr OperationList getCurrentOpcode() const noexcept { return currentOpcode_; }
private:
    OperationList currentOpcode_ = OperationList::Count;
    SplitWord32 size_{static_cast<uint32_t>(E::Count) };
};


inline void
performNullWrite(const SplitWord32&, const Channel0Value&, byte, uint16_t) noexcept {
}
inline uint16_t
performNullRead(const SplitWord32&, const Channel0Value&, byte) noexcept {
    return 0;
}


class SerialDevice : public OperatorPeripheral<SerialDeviceOperations> {
    public:
        ~SerialDevice() override = default;
        bool begin() noexcept override;
        void setBaudRate(uint32_t baudRate) noexcept;
        [[nodiscard]] constexpr auto getBaudRate() const noexcept { return baud_; }
    protected:
        uint16_t extendedRead(const Channel0Value& m0) const noexcept override ;
        void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept override;
    private:
        uint32_t baud_ = 115200;
};
class InfoDevice : public OperatorPeripheral<InfoDeviceOperations> {
    public:
        ~InfoDevice() override = default;
    protected:
        uint16_t extendedRead(const Channel0Value& m0) const noexcept override ;
        void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept override;
};
enum class TimerDeviceOperations {
    Available,
    Size,
    UnixTime,
    SystemTimerComparisonValue,
    SystemTimerPrescalar,
    Count,
};

class TimerDevice : public OperatorPeripheral<TimerDeviceOperations> {
    public:
        using Parent = OperatorPeripheral<TimerDeviceOperations>;
        ~TimerDevice() override = default;
        bool begin() noexcept override;
        bool available() const noexcept override { return available_; }
        void startTransaction(const SplitWord32& addr) noexcept override;
    protected:
        uint16_t extendedRead(const Channel0Value& m0) const noexcept override ;
        void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept override;
    private:
        RTC_DS1307 rtc;
        bool available_ = false;
        SplitWord32 unixtimeCopy_{0};
};

void sendToDazzler(uint8_t character) noexcept;
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
