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

#ifndef SXCHIPSET_TYPE103_MCP23S17_H
#define SXCHIPSET_TYPE103_MCP23S17_H
#include <Arduino.h>
#include "Pinout.h"

namespace MCP23S17 {
    /**
     * @brief The set of registers exposed by the MCP23S17 in the default bank mode
     */
    enum class Registers : byte {
        IODIRA = 0,
        IODIRB,
        IPOLA,
        IPOLB,
        GPINTENA,
        GPINTENB,
        DEFVALA,
        DEFVALB,
        INTCONA,
        INTCONB,
        IOCONA_,
        IOCONB_,
        GPPUA,
        GPPUB,
        INTFA,
        INTFB,
        INTCAPA,
        INTCAPB,
        GPIOA,
        GPIOB,
        OLATA,
        OLATB,
        OLAT = OLATA,
        GPIO = GPIOA,
        IOCON = IOCONA_,
        IODIR = IODIRA,
        INTCAP = INTCAPA,
        INTF = INTFA,
        GPPU = GPPUA,
        INTCON = INTCONA,
        DEFVAL = DEFVALA,
        GPINTEN = GPINTENA,
        IPOL = IPOLA,
    };
    constexpr uint16_t AllInput16 = 0xFFFF;
    constexpr uint8_t AllInput8 = 0xFF;
    constexpr uint8_t AllOutput8 = 0;
    constexpr uint16_t AllOutput16 = 0;
    union IOCON {
        uint8_t whole;
        struct {
            uint8_t unused : 1;
            /**
             * @brief Set the polarity of the interrupt output pin; 1 is active-high, 0 is active-low; only applies when ODR is 0
             */
            uint8_t intpol: 1;
            /**
             * @brief Set to 1 to configure the interrupt pin as an open-drain output, zero means active driver output
             */
            uint8_t odr : 1;
            /**
             * @brief Writing 1 enables the hardware address pins; Allows up to 8 devices to share one chip select line via hardware address biasing
             */
            uint8_t haen : 1;
            /**
             * @brief Slew rate control bit for SDA output (i2c only)
             */
             uint8_t disslw : 1;
            /**
             * @brief Sequential operation mode bit; Writing 1 disables the address pointer incrementing to _different_ registers,
             * instead the address pointer will stay on the current register (will still auto increment across the register pair if
             * all registers are in the same bank); Writing 0 will cause the address pointer to walk through the address space as bytes are
             * sent in the same transaction.
             */
             uint8_t seqop : 1;

             /**
              * @brief Interrupt pins A and B will be mirrored? Writing 1 will cause A and B to be "or-d" together internally (effectively
              * turns two 8-bit port interrupts into a single 16-bit interrupt); Writing a 0 will cause the interrupt lines A and B to be
              * bound to their corresponding port (they will be independent).
              */
             uint8_t mirror : 1;
             /**
              * @brief Controls how registers are addressed. Writing 1 will cause the each port to have it's register addresses separated off.
              * Writing a zero will cause the two 8-bit registers to become 16-bit registers in the memory map.
              */
             uint8_t bank : 1;
        } bits;

        constexpr auto treatingDeviceAsTwo8BitPorts() const noexcept { return bits.bank != 0; }
        constexpr auto treatingDeviceAsOne16bitPort() const noexcept { return bits.bank == 0; }
        constexpr auto interruptPinsAreMirroed() const noexcept { return bits.mirror != 0; }
        constexpr auto interruptPinsAreIndependent() const noexcept { return bits.mirror == 0;}
        constexpr auto sequentialOperationModeDisabled() const noexcept { return bits.seqop != 0; }
        constexpr auto sequentialOperationModeEnabled() const noexcept { return bits.seqop == 0; }
        constexpr auto slewRateDisabled() const noexcept { return bits.disslw != 0; }
        constexpr auto slewRateEnabled() const noexcept { return bits.disslw == 0; }
        constexpr auto hardwareAddressingEnabled() const noexcept { return bits.haen != 0; }
        constexpr auto hardwareAddressingDisabled() const noexcept { return bits.haen == 0; }
        constexpr auto interruptPinsAreOpenDrain() const noexcept { return bits.odr != 0; }
        constexpr auto interruptPinsAreActiveDriver() const noexcept { return bits.odr == 0; }
        constexpr auto interruptPolarityIsActiveHigh() const noexcept { return bits.intpol != 0; }
        constexpr auto interruptPolarityIsActiveLow() const noexcept { return bits.intpol == 0; }
        constexpr auto getRegister() const noexcept { return whole; }
        explicit IOCON(uint8_t value = 0) : whole(value) { }
        void treatDeviceAsTwo8BitPorts() noexcept { bits.bank = 1; }
        void treatDeviceAsOne16BitPort() noexcept { bits.bank = 0; }
        void mirrorInterruptPins() noexcept { bits.mirror = 1; }
        void makeInterruptPinsIndependent() noexcept { bits.mirror = 0; }
        void disableSequentialOperation() noexcept { bits.seqop = 1; }
        void enableSequentialOperation() noexcept { bits.seqop = 0; }
        void enableSlewRate() noexcept { bits.disslw = 0; }
        void disableSlewRate() noexcept { bits.disslw = 1; }
        void enableHardwareAddressing() noexcept { bits.haen = 1; }
        void disableHardwareAddressing() noexcept { bits.haen = 0; }
        void configureInterruptsAsOpenDrain() noexcept { bits.odr = 1; }
        void configureInterruptsAsActiveDriver() noexcept { bits.odr = 0; }
        void interruptIsActiveHigh() noexcept { bits.intpol = 1; }
        void interruptIsActiveLow() noexcept { bits.intpol = 0; }
    };
    /**
     * @brief The MCP23S17 devices connected to the single select pin. The MCP23S17 uses biased addressing to allow up to 8 io expanders to
     * use the same enable line. When hardware addressing is enabled, the address described via biasing is encoded into the spi data stream
     * in the first byte transmitted. This enum class is meant to make construction the read/write opcodes trivial
     */
    enum class HardwareDeviceAddress : byte {
        Device0 = 0b0000,
        Device1 = 0b0010,
        Device2 = 0b0100,
        Device3 = 0b0110,
        Device4 = 0b1000,
        Device5 = 0b1010,
        Device6 = 0b1100,
        Device7 = 0b1110,
    };
    enum class ExpanderPin {
        Pin0,
        Pin1,
        Pin2,
        Pin3,
        Pin4,
        Pin5,
        Pin6,
        Pin7,
        Pin8,
        Pin9,
        Pin10,
        Pin11,
        Pin12,
        Pin13,
        Pin14,
        Pin15,
        Count,
    };
    constexpr byte generateReadOpcode(uint8_t address, bool shift) noexcept {
        return 0b0100'0001 | (shift ? (address << 1) : address);
    }
    constexpr byte generateReadOpcode(HardwareDeviceAddress address) noexcept {
        return generateReadOpcode(static_cast<uint8_t>(address), false);
    }
    constexpr byte generateWriteOpcode(uint8_t address, bool shift) noexcept {
        return 0b0100'0000 | (shift ? (address << 1) : address);
    }
    constexpr byte generateWriteOpcode(HardwareDeviceAddress address) noexcept {
        return generateWriteOpcode(static_cast<uint8_t>(address), false);
    }
    template<HardwareDeviceAddress addr>
    constexpr auto ReadOpcode_v = generateReadOpcode(addr);
    template<HardwareDeviceAddress addr>
    constexpr auto WriteOpcode_v = generateWriteOpcode(addr);
    template<HardwareDeviceAddress address, Registers opcode, Pin pin = Pin::GPIOSelect>
    inline uint8_t read8() noexcept {
        digitalWrite<pin, LOW>();
        SPDR = ReadOpcode_v<address>;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(opcode) ;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = 0;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        auto result = SPDR;
        digitalWrite<pin, HIGH>();
        return result;
    }
    template<HardwareDeviceAddress address, Registers opcode, Pin pin = Pin::GPIOSelect>
    inline uint16_t read16() noexcept {
        uint16_t output = 0;
        digitalWrite<pin, LOW>();
        SPDR = ReadOpcode_v<address>;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(opcode) ;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = 0;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        auto lower = SPDR;
        SPDR = 0;
        asm volatile("nop");
        {
            output = lower;
        }
        while (!(SPSR & _BV(SPIF))) ; // wait
        auto value = SPDR;
        output |= (static_cast<uint16_t>(value) << 8);
        digitalWrite<pin, HIGH>();
        return output;
    }
    template<HardwareDeviceAddress addr, Registers opcode, Pin pin = Pin::GPIOSelect>
    inline void write8(byte value) noexcept {
        digitalWrite<pin, LOW>();
        SPDR = WriteOpcode_v<addr>;
        /*
         * The following NOP introduces a small delay that can prevent the wait
         * loop form iterating when running at the maximum speed. This gives
         * about 10% more speed, even if it seems counter-intuitive. At lower
         * speeds it is unnoticed.
         */
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(opcode);
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = value;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        digitalWrite<pin, HIGH>();
    }
    template<HardwareDeviceAddress addr, Registers opcode, Pin pin = Pin::GPIOSelect>
    inline void write16(uint16_t v) noexcept {
        digitalWrite<pin, LOW>();
        SPDR = WriteOpcode_v<addr>;
        asm volatile("nop");
        uint8_t lower = v;
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(opcode);
        asm volatile("nop");
        uint8_t upper = static_cast<uint8_t>(v >> 8);
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = lower;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = upper;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        digitalWrite<pin, HIGH>();
    }
    /**
     * @brief Read all 16 GPIOs of an io expander
     * @tparam addr The io expander to read from
     * @return The contents of the GPIO register pair
     */
    template<HardwareDeviceAddress addr, Pin pin = Pin::GPIOSelect>
    inline auto readGPIO16() noexcept {
        return read16<addr, Registers::GPIO, pin>();
    }
    /**
     * @brief Set all 16 GPIOs of an io expander
     * @tparam addr The io expander to write to
     * @tparam standalone When true, wrap the call in a begin/endTransaction call. When false omit them because you are doing many spi operations back to back and the begin/end is handled manually (default true)
     * @param value The value to set the gpios to
     */
    template<HardwareDeviceAddress addr, Pin pin = Pin::GPIOSelect>
    inline void writeGPIO16(uint16_t value) noexcept {
        write16<addr, Registers::GPIO, pin>(value);
    }
    /**
     * @brief Describe the directions of all 16 pins on a given io expander.
     * @tparam addr The io expander to update
     * @tparam standalone When true, wrap the call in a begin/endTransaction call. When false omit them because you are doing many spi operations back to back and the begin/end is handled manually (default true)
     * @param value The 16-bit direction mask to write to the io expander (a 1 means input, a 0 means output)
     */
    template<HardwareDeviceAddress addr, Pin pin = Pin::GPIOSelect>
    inline void writeDirection(uint16_t value) noexcept {
        write16<addr, Registers::IODIR, pin>(value) ;
    }
    template<HardwareDeviceAddress addr>
    inline auto readDirection() noexcept {
        return read16<addr, Registers::IODIR, Pin::GPIOSelect>();
    }
    template<HardwareDeviceAddress addr>
    IOCON readIOCON() noexcept {
        return IOCON(read8<addr, Registers::IOCON, Pin::GPIOSelect>());
    }

    template<HardwareDeviceAddress addr>
    void writeIOCON(const IOCON& value) noexcept {
        write8<addr, Registers::IOCON, Pin::GPIOSelect>(value.getRegister());
    }
} // end namespace MCP23S17
#endif //SXCHIPSET_TYPE103_MCP23S17_H
