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

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "Types.h"
#include "Pinout.h"
#include "MCP23S17.h"
#include "Wire.h"
#include "RTClib.h"
SdFat SD;
// the logging shield I'm using has a DS1307 RTC
enum class InstalledRTC {
    None,
    DS1307,
    DS3231,
    PCF8523,
    PCF8563,
    Micros,
};
RTC_DS1307 rtc1307;
RTC_DS3231 rtc3231;
RTC_PCF8523 rtc8523;
RTC_PCF8563 rtc8563;
volatile InstalledRTC activeRTC = InstalledRTC::None;

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
constexpr auto AddressUpper = MCP23S17::HardwareDeviceAddress::Device1;
constexpr auto AddressLower = MCP23S17::HardwareDeviceAddress::Device2;
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device3;
constexpr auto GPIOA_Lower = MCP23S17::HardwareDeviceAddress::Device4;
constexpr auto GPIOA_Upper = MCP23S17::HardwareDeviceAddress::Device5;
constexpr auto GPIOB_Lower = MCP23S17::HardwareDeviceAddress::Device6;
constexpr auto GPIOB_Upper = MCP23S17::HardwareDeviceAddress::Device7;

[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    pulse<Pin::Ready, LOW, HIGH>();
}
void 
configureReset() noexcept {
    // reset is always an output 
    auto theDirection = MCP23S17::read8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>();
    theDirection &= ~0b1;
    MCP23S17::write8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>(theDirection);
}
void
setSPI1Channel(byte index) noexcept {
    digitalWrite<Pin::SPI2_OFFSET0>(index & 0b001 ? HIGH : LOW);
    digitalWrite<Pin::SPI2_OFFSET1>(index & 0b010 ? HIGH : LOW);
    digitalWrite<Pin::SPI2_OFFSET2>(index & 0b100 ? HIGH : LOW);
}
void setInputChannel(byte value) noexcept {
    if (value & 0b1) {
        digitalWrite<Pin::SEL, HIGH>();
    } else {
        digitalWrite<Pin::SEL, LOW>();
    }
    asm volatile ("nop");
    asm volatile ("nop");
}
constexpr bool EnableDebugMode = false;
void handleTransaction() noexcept;

[[gnu::always_inline]] inline void 
doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}
void putCPUInReset() noexcept {
    doReset(LOW);
}
void pullCPUOutOfReset() noexcept {
    doReset(HIGH);
}
void configurePins() noexcept;
void setupIOExpanders() noexcept;
void installMemoryImage() noexcept;
uint16_t dataLinesDirection = MCP23S17::AllInput16;
uint16_t currentDataLinesValue = 0;
template<bool performFullMemoryTest>
void
setupPSRAM() noexcept {
    Serial.println(F("RUNNING PSRAM MEMORY TEST!"));
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    digitalWrite<Pin::PSRAM0, LOW>();
    SPI.transfer(0x66);
    digitalWrite<Pin::PSRAM0, HIGH>();
    // test the first 64k instead of the full 8 megabytes
    constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
    for (uint32_t i = 0; i < endAddress; i += 4) {
        union {
            uint32_t whole;
            uint8_t bytes[4];
        } container, result;
        container.whole = i;
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x02); // write
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[3]);
        digitalWrite<Pin::PSRAM0, HIGH>();
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x03); // read 
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        result.bytes[0] = SPI.transfer(0);
        result.bytes[1] = SPI.transfer(0);
        result.bytes[2] = SPI.transfer(0);
        result.bytes[3] = SPI.transfer(0);
        digitalWrite<Pin::PSRAM0, HIGH>();
        if (container.whole != result.whole) {
            Serial.print(F("MEMROY TEST FAILURE: W: 0x"));
            Serial.print(container.whole, HEX);
            Serial.print(F(" G: 0x"));
            Serial.println(result.whole, HEX);
            while (true) {
                // halt here
                delay(1000);
            }
        }
    }
    Serial.println(F("MEMORY TEST COMPLETE!"));
}
bool
trySetupDS1307() noexcept {
    if (rtc1307.begin()) {
        activeRTC = InstalledRTC::DS1307;
        if (!rtc1307.isrunning()) {
            rtc1307.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
    } else {
        return false;
    }
}
bool
trySetupDS3231() noexcept {
    if (rtc3231.begin()) {
        activeRTC = InstalledRTC::DS3231;
        if (!rtc3231.lostPower()) {
            rtc3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
    } else {
        return false;
    }
}
template<bool performCalibration = true>
bool
trySetupPCF8523() noexcept {
    if (rtc8523.begin()) {
        activeRTC = InstalledRTC::PCF8523;
        if (!rtc8523.lostPower()) {
            rtc8523.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        rtc8523.start();
        if constexpr (performCalibration) {
            // The PCF8523 can be calibrated for:
            //        - Aging adjustment
            //        - Temperature compensation
            //        - Accuracy tuning
            // The offset mode to use, once every two hours or once every minute.
            // The offset Offset value from -64 to +63. See the Application Note for calculation of offset values.
            // https://www.nxp.com/docs/en/application-note/AN11247.pdf
            // The deviation in parts per million can be calculated over a period of observation. Both the drift (which can be negative)
            // and the observation period must be in seconds. For accuracy the variation should be observed over about 1 week.
            // Note: any previous calibration should cancelled prior to any new observation period.
            // Example - RTC gaining 43 seconds in 1 week
            float drift = 43; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
            float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
            float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
            float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
                                     // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
            int offset = round(deviation_ppm / drift_unit);
            rtc8523.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
        }
        return true;
    } else {
        return false;
    }
}

bool
trySetupPCF8563() noexcept {
    if (rtc8563.begin()) {
        activeRTC = InstalledRTC::PCF8563;
        if (!rtc8563.lostPower()) {
            rtc8563.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        rtc8563.start();
        return true;
    } else {
        return false;
    }
}
void 
setupRTC() noexcept {
    // use short circuiting or to choose the first available rtc
    if (trySetupDS1307() || trySetupDS3231() || trySetupPCF8523() || trySetupPCF8563()) {
        Serial.print(F("Found RTC: "));
        switch (activeRTC) {
            case InstalledRTC::DS1307:
                Serial.println(F("DS1307"));
                break;
            case InstalledRTC::DS3231:
                Serial.println(F("DS3231"));
                break;
            case InstalledRTC::PCF8523:
                Serial.println(F("PCF8523"));
                break;
            case InstalledRTC::PCF8563:
                Serial.println(F("PCF8563"));
                break;
            case InstalledRTC::Micros:
                Serial.println(F("Internal micros counter"));
                break;
            default:
                Serial.println(F("Unknown"));
                break;
        }
    } else {
        Serial.println(F("No active RTC found!"));
    }
}
void 
setup() {
    Serial.begin(115200);
    setupRTC();
    SPI.begin();
    // setup the IO Expanders
    MCP23S17::IOCON reg;
    reg.makeInterruptPinsIndependent();
    reg.treatDeviceAsOne16BitPort();
    reg.enableHardwareAddressing();
    reg.interruptIsActiveLow();
    reg.configureInterruptsAsActiveDriver();
    reg.disableSequentialOperation();
    // at the start all of the io expanders will respond to the same address
    // so first just make sure we write out the initial iocon
    MCP23S17::writeIOCON<MCP23S17::HardwareDeviceAddress::Device0>(reg);
    // now make sure that everything is configured correctly initially
    MCP23S17::writeIOCON<DataLines>(reg);
    MCP23S17::writeDirection<DataLines>(MCP23S17::AllInput16);
    MCP23S17::write16<DataLines, MCP23S17::Registers::GPINTEN>(0xFFFF);
    MCP23S17::writeIOCON<AddressLower>(reg);
    MCP23S17::writeDirection<AddressLower>(MCP23S17::AllInput16);
    MCP23S17::write16<AddressLower, MCP23S17::Registers::GPINTEN>(0xFFFF);
    MCP23S17::writeIOCON<AddressUpper>(reg);
    MCP23S17::writeDirection<AddressUpper>(MCP23S17::AllInput16);
    MCP23S17::write16<AddressUpper, MCP23S17::Registers::GPINTEN>(0xFFFF);
    reg.mirrorInterruptPins();
    MCP23S17::writeIOCON<XIO>(reg);
    MCP23S17::writeDirection<XIO>(MCP23S17::AllInput16);
    reg.interruptIsActiveHigh();
    MCP23S17::writeIOCON<GPIOA_Lower>(reg);
    MCP23S17::writeDirection<GPIOA_Lower>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOA_Upper>(reg);
    MCP23S17::writeDirection<GPIOA_Upper>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOB_Lower>(reg);
    MCP23S17::writeDirection<GPIOB_Lower>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOB_Upper>(reg);
    MCP23S17::writeDirection<GPIOB_Upper>(MCP23S17::AllInput16);
    dataLinesDirection = MCP23S17::AllInput16;
    currentDataLinesValue = 0;
    MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
    // configure pins
    pinMode(Pin::HOLD, OUTPUT);
    pinMode(Pin::HLDA, INPUT);
    pinMode(Pin::GPIOSelect, OUTPUT);
    pinMode(Pin::CS2, OUTPUT);
    pinMode(Pin::SD_EN, OUTPUT);
    pinMode(Pin::PSRAM0, OUTPUT);
    pinMode(Pin::Ready, OUTPUT);
    pinMode(Pin::SPI2_OFFSET0, OUTPUT);
    pinMode(Pin::SPI2_OFFSET1, OUTPUT);
    pinMode(Pin::SPI2_OFFSET2, OUTPUT);
    pinMode(Pin::INT0_, OUTPUT);
    pinMode(Pin::SEL, OUTPUT);
    pinMode(Pin::INT3_, OUTPUT);
    pinMode(Pin::Capture0, INPUT);
    pinMode(Pin::Capture1, INPUT);
    pinMode(Pin::Capture2, INPUT);
    pinMode(Pin::Capture3, INPUT);
    pinMode(Pin::Capture4, INPUT);
    pinMode(Pin::Capture5, INPUT);
    pinMode(Pin::Capture6, INPUT);
    pinMode(Pin::Capture7, INPUT);
    configureReset();
    setSPI1Channel(0);
    digitalWrite<Pin::Ready, HIGH>();
    digitalWrite<Pin::HOLD, LOW>();
    digitalWrite<Pin::GPIOSelect, HIGH>();
    digitalWrite<Pin::CS2, HIGH>();
    digitalWrite<Pin::INT0_, HIGH>();
    digitalWrite<Pin::INT3_, HIGH>();
    digitalWrite<Pin::PSRAM0, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
    setInputChannel(0);
    putCPUInReset();
    while (!SD.begin(static_cast<byte>(Pin::SD_EN))) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    setupPSRAM<false>();
    setupCache();
    installMemoryImage();
    pullCPUOutOfReset();
    while (digitalRead<Pin::FAIL>() == LOW) {
        if (digitalRead<Pin::DEN>() == LOW) {
            break;
        }
    }
    while (digitalRead<Pin::FAIL>() == HIGH) {
        if (digitalRead<Pin::DEN>() == LOW) {
            break;
        }
    }
    Serial.println(F("STARTUP COMPLETE! BOOTING..."));
    // okay so we got past this, just start performing actions
    setInputChannel(0);
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction();
    setInputChannel(0);
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction();
    setInputChannel(0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    } else {
        Serial.println(F("BOOT SUCCESSFUL!"));
    }
}


void 
loop() {
    setInputChannel(0);
    asm volatile ("nop");
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction();
}

/**
 * @brief Hacked sdCsInit that assumes the only pin we care about is SD_EN, otherwise failure
 * @param pin
 */
void sdCsInit(SdCsPin_t pin) {
    if (static_cast<Pin>(pin) != Pin::SD_EN) {
        Serial.println(F("ERROR! sdCsInit provided sd pin which is not SD_EN"));
        Serial.print(F("Pin is "));
        Serial.println(static_cast<byte>(pin));
        while (true) {
            delay(100);
        }
    } else {
        pinMode(pin, OUTPUT);
    }
}

void sdCsWrite(SdCsPin_t, bool level) {
    digitalWrite<Pin::SD_EN>(level);
}


void 
installMemoryImage() noexcept {
    if (File memoryImage; !memoryImage.open("boot.sys", FILE_READ)) {
        Serial.println(F("Couldn't open boot.sys!"));
        while (true) {
            delay(1000);
        }
    } else {
        // write out to the data cache as we go along, when we do a miss then
        // we will be successful in writing out to main memory
        memoryImage.seekSet(0);
        Serial.println(F("installing memory image from sd"));
        constexpr auto BufferSize = 1024;
        uint8_t buffer[BufferSize];
        for (uint32_t i = 0, j = 0; i < memoryImage.size(); i += BufferSize, ++j) {
            while (memoryImage.isBusy());
            SplitWord32 currentAddressLine(i);
            auto numRead = memoryImage.read(buffer, BufferSize);
            if (numRead < 0) {
                SD.errorHalt();
            }
            memoryWrite(currentAddressLine, buffer, BufferSize);
            if ((j % 16) == 0) {
                Serial.print(F("."));
            }
        }
        memoryImage.close();
        Serial.println();
        Serial.println(F("transfer complete!"));
    }
}


inline void 
setDataLinesOutput(uint16_t value) noexcept {
    if (currentDataLinesValue != value) {
        currentDataLinesValue = value;
        MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
    }
}
using ReadOperation = uint16_t (*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte);
using WriteOperation = void(*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t);
template<bool isReadOperation>
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0, ReadOperation onRead, WriteOperation onWrite) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        Channel1Value c1(PINA);
        if constexpr (isReadOperation) {
            setDataLinesOutput(onRead(addr, m0, c1, offset));
        } else {
            onWrite(addr, m0, c1, offset, MCP23S17::readGPIO16<DataLines>());
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
uint16_t
performSerialRead_Fast(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    return Serial.read();
}

uint16_t
performSerialRead_Compact(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    uint16_t output = 0xFFFF;
    (void)Serial.readBytes(reinterpret_cast<byte*>(&output), sizeof(output));
    return output;
}
void
performSerialWrite_Fast(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t value) noexcept {
    Serial.write(static_cast<uint8_t>(value));
}

void
performSerialWrite_Compact(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t value) noexcept {
    Serial.write(reinterpret_cast<byte*>(&value), sizeof(value));
}
void
performNullWrite(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t) noexcept {
}
uint16_t
performNullRead(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    return 0;
}
template<bool isReadOperation>
inline void
handleSerialOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    switch (addr.getIOFunction<SerialGroupFunction>()) {
        case SerialGroupFunction::RWFast:
            genericIOHandler<isReadOperation>(addr, m0, performSerialRead_Fast, performSerialWrite_Fast);
            break;
        case SerialGroupFunction::RWCompact:
            genericIOHandler<isReadOperation>(addr, m0, performSerialRead_Compact, performSerialWrite_Compact);
            break;
        case SerialGroupFunction::Flush:
            Serial.flush();
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}

template<bool isReadOperation>
inline void 
handleIOOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    // When we are in io space, we are treating the address as an opcode which
    // we can decompose while getting the pieces from the io expanders. Thus we
    // can overlay the act of decoding while getting the next part
    // 
    // The W/~R pin is used to figure out if this is a read or write operation
    //
    // This system does not care about the size but it does care about where
    // one starts when performing a write operation
    switch (addr.getIOGroup()) {
        case IOGroup::Serial:
            handleSerialOperation<isReadOperation>(addr, m0);
            break;
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}
uint16_t getDataLines(const Channel1Value& c1) noexcept {
    return MCP23S17::readGPIO16<DataLines>();
}
template<bool isReadOperation>
void
handleCacheOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    // okay now we can service the transaction request since it will be going
    // to ram.
    auto& line = getCache().find(addr);
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        Channel1Value c1(PINA);
        if constexpr (isReadOperation) {
            // okay it is a read operation, so... pull a cache line out 
            auto value = line.getWord(offset);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tGot Value: 0x"));
                Serial.println(value, HEX);
            }
            setDataLinesOutput(value);
        } else {
            auto value = getDataLines(c1);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            // so we are writing to the cache
            line.setWord(offset, value, c1.getByteEnable());
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
[[gnu::always_inline]]
inline void waitForByteTransfer() noexcept {
    while (!(SPSR & _BV(SPIF))) ; // wait
}
enum class TransactionKind {
    // 0b00 -> cache + read
    // 0b01 -> cache + write
    // 0b10 -> io + read
    // 0b11 -> io + write
    CacheRead,
    CacheWrite,
    IORead,
    IOWrite,
};

inline TransactionKind getTransaction(Channel0Value m0, const SplitWord32& addr) noexcept {
    if (addr.isIOInstruction()) {
        if (m0.isReadOperation()) {
            return TransactionKind::IORead;
        } else {
            return TransactionKind::IOWrite;
        }
    } else {
        if (m0.isReadOperation()) {
            return TransactionKind::CacheRead;
        } else {
            return TransactionKind::CacheWrite;
        }
    }
}
void 
handleTransaction() noexcept {

    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    // grab the entire state of port A
    // update the address as a full 32-bit update for now
    SplitWord32 addr{0};

    digitalWrite<Pin::GPIOSelect, LOW>();
    SPDR = MCP23S17::ReadOpcode_v<AddressUpper>;
    asm volatile("nop");
    Channel0Value m0(PINA);
    waitForByteTransfer();
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO) ;
    asm volatile("nop");
    setInputChannel(1);
    waitForByteTransfer();
    SPDR = 0;
    asm volatile("nop");
    auto direction = m0.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    waitForByteTransfer();
    auto value = SPDR;
    SPDR = 0;
    asm volatile("nop");
    addr.bytes[3] = value;
    waitForByteTransfer();
    value = SPDR;
    digitalWrite<Pin::GPIOSelect, HIGH>();

    digitalWrite<Pin::GPIOSelect, LOW>();
    SPDR = MCP23S17::ReadOpcode_v<AddressLower>;
    asm volatile("nop");
    addr.bytes[2] = value;
    waitForByteTransfer();
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO) ;
    asm volatile("nop");
    auto updateDataLines = direction != dataLinesDirection;
    waitForByteTransfer();
    SPDR = 0;
    asm volatile("nop");
    auto target = getTransaction(m0, addr);
    waitForByteTransfer();
    value = SPDR;
    SPDR = 0;
    asm volatile("nop");
    addr.bytes[0] = value;
    waitForByteTransfer();
    addr.bytes[1] = SPDR;
    digitalWrite<Pin::GPIOSelect, HIGH>();
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target address: 0x"));
        Serial.println(addr.getWholeValue(), HEX);
        Serial.print(F("Operation: "));
        if (m0.isReadOperation()) {
            Serial.println(F("Read!"));
        } else {
            Serial.println(F("Write!"));
        }
    }
    if (updateDataLines) {
        dataLinesDirection = direction;
        MCP23S17::writeDirection<DataLines>(dataLinesDirection);
    }
    switch (target) {
        case TransactionKind::CacheRead:
            handleCacheOperation<true>(addr, m0);
            break;
        case TransactionKind::CacheWrite:
            handleCacheOperation<false>(addr, m0);
            break;
        case TransactionKind::IORead:
            handleIOOperation<true>(addr, m0);
            break;
        case TransactionKind::IOWrite:
            handleIOOperation<false>(addr, m0);
            break;
    }
    SPI.endTransaction();
}


namespace {
    size_t
    psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x02); // write
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
        digitalWrite<Pin::PSRAM0, HIGH>();
        return count;
    }
    size_t
    psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x03); //read 
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
        digitalWrite<Pin::PSRAM0, HIGH>();
        return count;
    }
}
size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead(baseAddress, bytes, count);
}
