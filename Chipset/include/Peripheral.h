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

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
constexpr auto AddressUpper = MCP23S17::HardwareDeviceAddress::Device1;
constexpr auto AddressLower = MCP23S17::HardwareDeviceAddress::Device2;
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device3;

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
    Count,
};

enum class EEPROMFunctions {
    Available,
    Size,
    RW,
    Count,
};
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
using ReadOperation = uint16_t (*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte);
using WriteOperation = void(*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t);
extern SplitWord16 previousValue;
extern uint16_t currentDataLinesValue;
template<bool busHeldOpen>
inline void 
setDataLinesOutput(uint16_t value) noexcept {
    if (currentDataLinesValue != value) {
        currentDataLinesValue = value;
        if constexpr (busHeldOpen) {
            SPDR = static_cast<byte>(value);
            asm volatile ("nop");
            auto next = static_cast<byte>(value >> 8);
            while (!(SPSR & _BV(SPIF))) ; // wait
            SPDR = next;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
        } else {
            MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
        }
    }
}
template<bool busHeldOpen>
inline uint16_t 
getDataLines(const Channel1Value& c1) noexcept {
    if (c1.channel1.dataInt != 0b11) {
        if constexpr (busHeldOpen) {
            SPDR = 0;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
            auto value = SPDR;
            SPDR = 0;
            asm volatile ("nop");
            previousValue.bytes[0] = value;
            while (!(SPSR & _BV(SPIF))) ; // wait
            previousValue.bytes[1] = SPDR;
        } else {
            previousValue.full = MCP23S17::readGPIO16<DataLines>();
        }
    }
    return previousValue.full;
}
template<bool isReadOperation>
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0, ReadOperation onRead, WriteOperation onWrite) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        auto c1 = readInputChannelAs<Channel1Value>();
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, m0, c1, offset));
        } else {
            onWrite(addr, m0, c1, offset, getDataLines<false>(c1));
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
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0) noexcept {
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
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0, ReadOperation onRead) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, m0, readInputChannelAs<Channel1Value>(), offset));
        } 
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
class Peripheral {
    public:
        virtual ~Peripheral() = default;
        template<bool isReadOperation>
        void handleExecution(const SplitWord32& addr, const Channel0Value& m0) noexcept {
            if constexpr (isReadOperation) {
                readOperation(addr, m0);
            } else {
                writeOperation(addr, m0);
            }
        }
        virtual void readOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept = 0;
        virtual void writeOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept = 0;
};
template<typename E>
class OperatorPeripheral : public Peripheral {
public:

    using OperationList = E;
    ~OperatorPeripheral() override = default;
    virtual bool available() const noexcept { return false; }
    virtual uint32_t size() const noexcept { return 0; }
    template<bool isReadOperation>
    void handleExecution(const SplitWord32& addr, const Channel0Value& m0) noexcept {
        switch (auto opcode = addr.getIOFunction<OperationList>(); opcode) {
            default:
                if constexpr (isReadOperation) {
                    handleExtendedReadOperation(addr, m0, opcode);
                } else {
                    handleExtendedWriteOperation(addr, m0, opcode);
                }
                break;
        }
    }
    void readOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept override {
        switch (auto opcode = addr.getIOFunction<OperationList>(); opcode) {
            default:
                handleExtendedReadOperation(addr, m0, opcode);
                break;
        }

    }
    void writeOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept override {
        switch (auto opcode = addr.getIOFunction<OperationList>(); opcode) {
            default:
                handleExtendedWriteOperation(addr, m0, opcode);
                break;
        }

    }
protected:
    virtual void handleExtendedReadOperation(const SplitWord32& addr, const Channel0Value& m0, OperationList value) noexcept = 0;
    virtual void handleExtendedWriteOperation(const SplitWord32& addr, const Channel0Value& m0, OperationList value) noexcept = 0;
};
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H
