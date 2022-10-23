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
#include <Wire.h>
#include <SdFat.h>
#include <ArduinoJson.h>
#include "Pinout.h"
#include "MCP23S17.h"
SdFat SD;
File ramFile;

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
void setSPI1Channel(byte index) noexcept {
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
}
void putCPUInReset() noexcept {
    setInputChannel(0);
    digitalWrite<Pin::Reset960, LOW>();
}
void pullCPUOutOfReset() noexcept {
    setInputChannel(0);
    digitalWrite<Pin::Reset960, HIGH>();
}
void configurePins() noexcept;
void setupIOExpanders() noexcept;
void setupCache() noexcept;
void installMemoryImage() noexcept;
void 
setup() {
    Serial.begin(115200);
    SPI.begin();
    setupIOExpanders();
    configurePins();
    setupCache();
    while (!SD.begin()) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
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
    // okay so we got past this, just start performing actions
}
union Channel0Value {
    explicit Channel0Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t den : 1;
        uint8_t w_r_ : 1;
        uint8_t fail : 1;
        uint8_t unused : 1;
        uint8_t addrInt : 4;
    } bits;
    constexpr bool isReadOperation() const noexcept { return bits.w_r_ == 0; }
    constexpr bool isWriteOperation() const noexcept { return bits.w_r_ != 0; }
};
union Channel1Value {
    explicit Channel1Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t byteEnable : 2;
        uint8_t blast : 1;
        uint8_t xioint : 1;
        uint8_t dataInt : 2;
        uint8_t ramIO : 1;
        uint8_t unused : 1;
    } bits;
};
template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];
constexpr auto OffsetSize = 4; // 16-byte line
constexpr auto TagSize = 7; // 8192 bytes divided into 16-byte
                                   // lines with 4 lines per set
                                   // (4-way)
constexpr auto KeySize = 32 - (OffsetSize + TagSize);
union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    constexpr SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    struct {
        uint32_t a0 : 1;
        uint32_t offset : 3;
        uint32_t rest : 28;
    } address;
    struct {
        uint32_t offset : OffsetSize;
        uint32_t tag : TagSize; 
        uint32_t key : KeySize;
    } cacheAddress;
    struct {
        uint32_t offset : 8;
        uint32_t key : 24;
    } ioDeviceAddress;
};

union SplitWord16 {
    uint16_t full;
    ElementContainer<uint16_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint16_t, uint8_t>; }
    constexpr SplitWord16() : full(0) { }
    constexpr explicit SplitWord16(uint16_t value) : full(value) { }
    constexpr explicit SplitWord16(uint8_t a, uint8_t b) : bytes{a, b} { }
};

struct CacheLine {
    virtual ~CacheLine() = default;
    virtual void clear() noexcept = 0;
    virtual void reset(SplitWord32 newAddress) noexcept = 0;
    virtual bool matches(SplitWord32 other) const noexcept = 0;
    virtual uint16_t getWord(byte offset) const noexcept = 0;
    virtual void setWord(byte offset, uint16_t value, bool enableLower, bool enableUpper) noexcept = 0;
    virtual void begin() noexcept { }
};
struct Cache {
    virtual ~Cache() = default;
    virtual void clear() noexcept = 0;
    virtual CacheLine& find(SplitWord32 address) noexcept = 0;
    virtual void begin() noexcept = 0;
};

struct SplitCache : public Cache {
    ~SplitCache() override = default;
    virtual Cache& findCache(uint8_t address) noexcept = 0;
    CacheLine& find(SplitWord32 address) noexcept override {
        return findCache(address.bytes[3]).find(address);
    }
};
SplitCache& getCache() noexcept;

void 
loop() {
    setInputChannel(0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    }
    while (digitalRead<Pin::DEN>() == HIGH);
    // grab the entire state of port A
    // update the address as a full 32-bit update for now
    SplitWord32 addr{0};
    // interleave operations into the accessing of address lines
    digitalWrite<Pin::GPIOSelect, LOW>();
    SPDR = MCP23S17::generateReadOpcode(AddressUpper);
    nop;
    Channel0Value m0(PINA);
    auto isReadOperation = m0.isReadOperation();
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO);
    nop;
    // read in this case means output since the i960 is Reading 
    // write in this case means input since the i960 is Writing
    byte directionBits = isReadOperation ? MCP23S17::AllOutput8 : MCP23S17::AllInput8;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = 0;
    nop;
    // switch to the second input channel since this will go on regardless
    setInputChannel(1);
    while (!(SPSR & _BV(SPIF))) ;
    auto result = SPDR;
    SPDR = 0;
    nop;
    addr.bytes[3] = result;
    auto& targetCache = getCache().findCache(result);
    while (!(SPSR & _BV(SPIF))) ;
    digitalWrite<Pin::GPIOSelect, HIGH>();
    digitalWrite<Pin::GPIOSelect, LOW>();
    result = SPDR;
    SPDR = MCP23S17::generateReadOpcode(AddressLower);
    nop;
    addr.bytes[2] = result;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = 0;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    result = SPDR;
    SPDR = 0;
    nop;
    addr.bytes[0] = result;
    while (!(SPSR & _BV(SPIF))) ;
    addr.bytes[1] = SPDR;
    digitalWrite<Pin::GPIOSelect, HIGH>();

    /// @todo implement optimization to only update this if necessary
    // set data lines direction
    digitalWrite<Pin::GPIOSelect, LOW>();
    SPDR = MCP23S17::generateWriteOpcode(DataLines);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::IODIR);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = directionBits;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = directionBits;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    digitalWrite<Pin::GPIOSelect, HIGH>();
    // okay now we can service the transaction request 
    for (byte offset = addr.address.offset; offset < 8 /* words per transaction */; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        Channel1Value c1(PINA);
        if (m0.isReadOperation()) {

        }
        /// @todo insert handler code here
        digitalWrite<Pin::Ready, LOW>();
        digitalWrite<Pin::Ready, HIGH>();
        if (isBurstLast) {
            setInputChannel(0);
            break;
        }
    }
}

void 
configurePins() noexcept {
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
    pinMode(Pin::Reset960, OUTPUT);
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
}
void
setupIOExpanders() noexcept {
    
}

/**
 * @brief Hacked sdCsInit that assumes the only pin we care about is SD_EN, otherwise failure
 * @param pin
 */
void sdCsInit(SdCsPin_t pin) {
    if (static_cast<Pin>(pin) != Pin::SD_EN) {
        Serial.println(F("ERROR! sdCsInit provided sd pin which is not SD_EN"));
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

}
void 
doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}

[[gnu::always_inline]] 
inline void 
digitalWrite(Pin pin, decltype(LOW) value) noexcept { 
    if (isPhysicalPin(pin)) {
        if (auto &thePort = getOutputRegister(pin); value == LOW) {
            thePort = thePort & ~getPinMask(pin);
        } else {
            thePort = thePort | getPinMask(pin);
        }
    } else {
        switch (pin) {
            case Pin::SPI2_EN0:
            case Pin::SPI2_EN1:
            case Pin::SPI2_EN2:
            case Pin::SPI2_EN3:
            case Pin::SPI2_EN4:
            case Pin::SPI2_EN5:
            case Pin::SPI2_EN6:
            case Pin::SPI2_EN7:
                digitalWrite(Pin::CS2, value);
                break;
            case Pin::Reset960: 
                doReset(value);
                break;
            default:
                digitalWrite(static_cast<byte>(pin), value); 
                break;
        }
    }
}
[[gnu::always_inline]] 
inline void pinMode(Pin pin, decltype(INPUT) direction) noexcept {
    if (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } else if (pin == Pin::Reset960) {
        auto theDirection = MCP23S17::read8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>();
        if (direction == INPUT || direction == INPUT_PULLUP) {
            theDirection |= 0b1;
        } else if (direction == OUTPUT) {
            theDirection &= ~0b1;
        }
        MCP23S17::write8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>(theDirection);
    }
}
void memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
void memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;

struct DataCacheLine : public CacheLine {
    static constexpr auto NumberOfWords = 8;
    static constexpr auto NumberOfDataBytes = sizeof(SplitWord16)*NumberOfWords;
    ~DataCacheLine() override = default;
    union {
        uint32_t reg;
        struct {
            uint32_t key : KeySize;
            uint32_t valid_ : 1;
            uint32_t dirty_ : 1;
        };
    } metadata;
    static_assert(sizeof(metadata) == sizeof(uint32_t), "Too many flags specified for metadata");
    SplitWord16 words[NumberOfWords];
    uint16_t getWord(byte offset) const noexcept override {
        return words[offset & 0b111].full;
    }
    void clear() noexcept override {
        metadata.reg = 0;
        for (int i = 0; i < NumberOfWords; ++i) {
            words[i].full = 0;
        }
    }
    bool matches(SplitWord32 other) const noexcept override {
        return metadata.valid_ && (other.cacheAddress.key == metadata.key);
    }
    void reset(SplitWord32 newAddress) noexcept override {
        if (metadata.valid_ && metadata.dirty_) {
            auto copy = newAddress;
            copy.cacheAddress.offset = 0;
            copy.cacheAddress.key = metadata.key;
            memoryWrite(copy, reinterpret_cast<byte*>(words), NumberOfDataBytes);
        }
        metadata.valid_ = true;
        metadata.dirty_ = false;
        metadata.key = newAddress.cacheAddress.key;
        auto copy2 = newAddress;
        copy2.cacheAddress.offset = 0;
        memoryRead(copy2, reinterpret_cast<byte*>(words), NumberOfDataBytes);
    }
    void setWord(byte offset, uint16_t value, bool enableLower, bool enableUpper) noexcept override {
        if (enableLower) {
            words[offset & 0b111].bytes[0] = value;
            metadata.dirty_ = true;
        }
        if (enableUpper) {
            words[offset & 0b111].bytes[1] = value >> 8;
            metadata.dirty_ = true;
        }
    }

};
struct CacheSet {
    virtual ~CacheSet() = default;
    virtual CacheLine& find(SplitWord32 address) noexcept = 0;
    virtual void clear() noexcept = 0;
    virtual void begin() noexcept = 0;
};
struct DataCacheSet : public CacheSet {
    static constexpr auto NumberOfLines = 4;
    ~DataCacheSet() override = default;
    void begin() noexcept override {
        for (int i = 0; i < NumberOfLines; ++i) {
            lines[i].begin();
        }
    }
    CacheLine& find(SplitWord32 address) noexcept override {
        for (int i = 0; i < NumberOfLines; ++i) {
            if (lines[i].matches(address)) {
                return lines[i];
            }
        }
        auto& target = lines[replacementIndex_];
        ++replacementIndex_;
        replacementIndex_ %= NumberOfLines;
        target.reset(address);
        return target;
    }
    void clear() noexcept override {
        replacementIndex_ = 0;
        for (int i = 0; i < NumberOfLines; ++i) {
            lines[i].clear();
        }
    }
    DataCacheLine lines[NumberOfLines];
    byte replacementIndex_ = 0;
};
struct DataCache : public Cache{
    ~DataCache() override = default;
    void clear() noexcept override {
        for (int i = 0; i < 128; ++i) {
            cache[i].clear();
        }
    }
    CacheLine& find(SplitWord32 address) noexcept override {
        return cache[address.cacheAddress.tag].find(address);
    }
    void begin() noexcept override {
        for (auto& set : cache) {
            set.begin();
        }
    }
    DataCacheSet cache[128];
};
struct IODevice : public CacheLine {
    IODevice(uint32_t address) noexcept : addr_(address) { }
    ~IODevice() override = default;
    void reset(SplitWord32 ) noexcept override { }
    void clear() noexcept override {

    }
    bool matches(SplitWord32 other) const noexcept override { return other.ioDeviceAddress.key == addr_.ioDeviceAddress.key; }
    private:
        SplitWord32 addr_;
};
struct IOSink final : public CacheLine {
    ~IOSink() override = default;
    void reset(SplitWord32) noexcept override { }
    void clear() noexcept override { }
    void begin() noexcept override { }
    bool matches(SplitWord32) const noexcept override { return true; }
    uint16_t getWord(byte) const noexcept override { return 0; }
    void setWord(byte, uint16_t, bool, bool) noexcept override { } 
};
struct ConfigurationSpace : public IODevice {
    public:
        ConfigurationSpace() noexcept : IODevice(0xFE00'0000) { }
        ~ConfigurationSpace() override = default;
        void begin() noexcept override  {
            if (File configurationSpaceBinary; !configurationSpaceBinary.open("cfgspace.bin", FILE_READ)) {
                Serial.println(F("Could not open cfgspace.bin!"));
                while (true) {
                    delay(1000);
                }
            } else {
                // load the addresses from SDCard and use it to setup the address space
                // on startup
                configurationSpaceBinary.seekSet(0);
                configurationSpaceBinary.read(reinterpret_cast<byte*>(words), 256);
                configurationSpaceBinary.close();
            }
        }
        uint16_t getWord(byte offset) const noexcept override {
            // lowest bit ignored
            return words[offset & 0b0111'1111].full;
        }
        void setWord(byte, uint16_t, bool, bool) noexcept override {
            // ignore attempts to write values out 
        }
        constexpr auto getSerialAddress() const noexcept { return addrs[0].full; }
        constexpr auto getSDCtl() const noexcept { return addrs[1].full; }
        constexpr auto getSDFilesStart() const noexcept { return addrs[2].full; }
        constexpr auto getSeesawCtl() const noexcept { return addrs[3].full; }
        constexpr auto getDisplayCtl() const noexcept { return addrs[4].full; }
        constexpr auto getRtcCtl() const noexcept { return addrs[5].full; }

    private:
        union {
            uint8_t bytes[256];
            SplitWord16 words[sizeof(bytes)/sizeof(SplitWord16)];
            SplitWord32 addrs[sizeof(bytes)/sizeof(SplitWord32)];
        };
};
struct SerialDevice : public IODevice {
    public:
        SerialDevice(uint32_t address) noexcept : IODevice(address) { }
        ~SerialDevice() override = default;
        void begin() noexcept override  { }

        uint16_t getWord(byte offset) const noexcept override {
            switch (offset & 0b0111'1111) {
                case 0: //read port
                    return Serial.read();
                default:
                    return 0;
            }
        }
        void setWord(byte offset, uint16_t value, bool, bool) noexcept override {
            switch (offset & 0b0111'1111) {
                case 0: // write port
                    Serial.write(value);
                    break;
                default:
                    break;
            }
        }
};
struct IOSpace : public Cache {
    ~IOSpace() override = default;
    void clear() noexcept override {
        cfgSpace.clear();
        for (auto* ptr : devices) {
            ptr->clear();
        }
    }
    CacheLine& find(SplitWord32 address) noexcept override {
        // assume that if we got here that address is in correct range :)
        if (cfgSpace.matches(address)) {
            return cfgSpace;
        } else {
            for (auto* dev : devices) {
                if (dev->matches(address)) {
                    return *dev;
                }
            }
            return fallback_;
        }
    }
    void begin() noexcept override {
        if (!initialized_) {
            initialized_ = true;
            cfgSpace.begin();
            devices[0] = new SerialDevice(cfgSpace.getSerialAddress());
            for (int i = 1; i < 64; ++i) {
                devices[i] = new IOSink();
            }
            for (auto* ptr : devices) {
                ptr->begin();
            }
        }
    }
    private:
        bool initialized_ = false;
        ConfigurationSpace cfgSpace;
        CacheLine* devices[64] { nullptr } ;
        IOSink fallback_;
};
struct MultipartCache : public SplitCache {
    ~MultipartCache() override = default;
    void clear() noexcept override {
        io_.clear();
        cache_.clear();
    }
    /**
     * @brief Return the cache associated with the upper most 8-bits
     */
    Cache& findCache(uint8_t topLevel) noexcept {
        if (topLevel == 0xFE) {
            return io_;
        } else {
            return cache_;
        }
    }
    CacheLine& find(SplitWord32 address) noexcept override {
        return findCache(address.bytes[3]).find(address);
    }
    void begin() noexcept {
        io_.begin();
        cache_.begin();
    }

    IOSpace io_;
    DataCache cache_;
};
MultipartCache cache;
void 
setupCache() noexcept {
    if (!ramFile.open("ram.bin", FILE_WRITE)) {
        Serial.println(F("Could not open ram.bin!"));
        while (true) {
            delay(1000);
        }
    }
    cache.begin();
    cache.clear();
}
void 
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    ramFile.seekSet(baseAddress.full);
    ramFile.write(bytes, count);
}
void 
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    ramFile.seekSet(baseAddress.full);
    ramFile.read(bytes, count);
    
}

SplitCache& 
getCache() noexcept {
    return cache;
}
