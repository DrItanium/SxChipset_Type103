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
#include "Types.h"
#include "MCP23S17.h"
#include "Pinout.h"
#include "RTClib.h"

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
/**
 * @brief Onboard device to control reset and other various features
 */
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device7;

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
    return T{PINA};
}
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    pulse<Pin::Ready, LOW, HIGH>();
}
[[gnu::always_inline]] 
inline void
interruptI960() noexcept {
    pulse<Pin::INT0_, LOW, HIGH>();
}
using ReadOperation = uint16_t (*)(const SplitWord32&, const Channel0Value&, byte);
using WriteOperation = void(*)(const SplitWord32&, const Channel0Value&, byte, uint16_t);
extern SplitWord16 previousValue;
extern uint16_t currentDataLinesValue;
template<bool busHeldOpen>
[[gnu::always_inline]] 
inline void 
setDataLinesOutput(uint16_t value) noexcept {
    if (currentDataLinesValue != value) {
        currentDataLinesValue = value;
        if constexpr (busHeldOpen) {
#ifdef AVR_SPI_AVAILABLE
            SPDR = static_cast<byte>(value);
            asm volatile ("nop");
            auto next = static_cast<byte>(value >> 8);
            while (!(SPSR & _BV(SPIF))) ; // wait
            SPDR = next;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
#else
            SPI.transfer(static_cast<byte>(value));
            SPI.transfer(static_cast<byte>(value >> 8));
#endif
        } else {
            MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
        }
    }
}
template<bool busHeldOpen, bool ignoreInterrupts = true>
[[gnu::always_inline]] 
inline uint16_t 
getDataLines(const Channel0Value& c1) noexcept {
    if (ignoreInterrupts || c1.dataInterruptTriggered()) {
        if constexpr (busHeldOpen) {
#ifdef AVR_SPI_AVAILABLE
            SPDR = 0;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
            auto value = SPDR;
            SPDR = 0;
            asm volatile ("nop");
            previousValue.bytes[0] = value;
            while (!(SPSR & _BV(SPIF))) ; // wait
            previousValue.bytes[1] = SPDR;
#else
            previousValue.bytes[0] = SPI.transfer(0);
            previousValue.bytes[1] = SPI.transfer(0);
#endif
        } else {
            previousValue.full = MCP23S17::readGPIO16<DataLines>();
        }
    }
    return previousValue.full;
}
template<bool isReadOperation>
inline void
genericIOHandler(const SplitWord32& addr, ReadOperation onRead, WriteOperation onWrite) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        auto m0 = readInputChannelAs<Channel0Value>();
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, m0, offset));
        } else {
            onWrite(addr, m0, offset, getDataLines<false>(m0));
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
/**
 * @brief Fallback implementation when the io request doesn't map to any one
 * function
 */
template<bool isReadOperation>
inline void
genericIOHandler(const SplitWord32& addr) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(0);
        } 
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}

template<bool isReadOperation>
inline void
genericIOHandler(const SplitWord32& addr, ReadOperation onRead) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, readInputChannelAs<Channel0Value>(), offset));
        } 
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}

template<bool isReadOperation, typename T>
inline void
genericIOHandler(const SplitWord32& addr, T onRead) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, readInputChannelAs<Channel0Value>(), offset));
        } 
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
inline void
readOnlyDynamicValue(const SplitWord32& addr, uint16_t value) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        setDataLinesOutput<false>(value);
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
inline void
readOnlyDynamicValue(const SplitWord32& addr, uint32_t value) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if (offset & 0b1) {
            setDataLinesOutput<false>(static_cast<uint16_t>(value >> 16));
        } else {
            setDataLinesOutput<false>(static_cast<uint16_t>(value));
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}

inline void
readOnlyDynamicValue(const SplitWord32& addr, uint64_t value) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        switch (offset & 0b11) {
            case 0b00:
                setDataLinesOutput<false>(value);
                break;
            case 0b01:
                setDataLinesOutput<false>(value >> 16);
                break;
            case 0b10:
                setDataLinesOutput<false>(value >> 32);
                break;
            case 0b11:
                setDataLinesOutput<false>(value >> 48);
                break;
            default:
                setDataLinesOutput<false>(0);
                break;

        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
inline void
readOnlyDynamicValue(const SplitWord32& addr, bool value) noexcept {
    readOnlyDynamicValue(addr, value ? 0xFFFF : 0x0);
}



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
class Peripheral {
    public:
        virtual ~Peripheral() = default;
        template<bool isReadOperation>
        void handleExecution(const SplitWord32& addr) noexcept {
            if constexpr (isReadOperation) {
                readOperation(addr);
            } else {
                writeOperation(addr);
            }
        }
        virtual void readOperation(const SplitWord32& addr) noexcept = 0;
        virtual void writeOperation(const SplitWord32& addr) noexcept = 0;
        virtual bool begin() noexcept { return true; }
};
template<typename E>
class OperatorPeripheral : public Peripheral {
public:

    using OperationList = E;
    ~OperatorPeripheral() override = default;
    virtual bool available() const noexcept { return true; }
    virtual uint32_t size() const noexcept { return static_cast<uint32_t>(E::Count); }
    void readOperation(const SplitWord32& addr) noexcept override {
        switch (auto opcode = addr.getIOFunction<OperationList>(); opcode) {
            case E::Available:
                readOnlyDynamicValue(addr, available());
                break;
            case E::Size:
                readOnlyDynamicValue(addr, size());
                break;
            default:
                if (validOperation(opcode)) {
                    handleExtendedReadOperation(addr, opcode);
                } else {
                    genericIOHandler<true>(addr);
                }
                break;
        }

    }
    void writeOperation(const SplitWord32& addr) noexcept override {
        switch (auto opcode = addr.getIOFunction<OperationList>(); opcode) {
            case E::Available:
            case E::Size:
                genericIOHandler<false>(addr);
                break;
            default:
                if (validOperation(opcode)) {
                    handleExtendedWriteOperation(addr, opcode);
                } else {
                    genericIOHandler<false>(addr);
                }
                break;
        }

    }
protected:
    virtual void handleExtendedReadOperation(const SplitWord32& addr, OperationList value) noexcept = 0;
    virtual void handleExtendedWriteOperation(const SplitWord32& addr, OperationList value) noexcept = 0;
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
        void handleExtendedReadOperation(const SplitWord32& addr, SerialDeviceOperations value) noexcept override;
        void handleExtendedWriteOperation(const SplitWord32& addr, SerialDeviceOperations value) noexcept override;
    private:
        uint32_t baud_ = 115200;
};
class InfoDevice : public OperatorPeripheral<InfoDeviceOperations> {
    public:
        ~InfoDevice() override = default;
    protected:
        void handleExtendedReadOperation(const SplitWord32& addr, InfoDeviceOperations value) noexcept override;
        void handleExtendedWriteOperation(const SplitWord32& addr, InfoDeviceOperations value) noexcept override;
};
enum class RTCDeviceOperations {
    Available,
    Size,
    UnixTime,
    Count,
};

class RTCDevice : public OperatorPeripheral<RTCDeviceOperations> {
    public:
        ~RTCDevice() override = default;
        bool begin() noexcept override;
        bool available() const noexcept override { return available_; }
    protected:
        void handleExtendedReadOperation(const SplitWord32& addr, RTCDeviceOperations value) noexcept override;
        void handleExtendedWriteOperation(const SplitWord32& addr, RTCDeviceOperations value) noexcept override;
    private:
        RTC_DS1307 rtc;
        bool available_ = false;
};
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
