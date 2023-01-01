/**
* i960SxChipset_Type103
* Copyright (c) 2022-2023, Joshua Scoggins
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include "xmem.h"
#include "BankSelection.h"
constexpr auto PSRAMEnable = 2;
constexpr auto SDPin = 10;
SdFat SD;
#if 0
void setup() {
    Serial.begin(115200);
    SPI.begin();
    enableXMEM();
    Wire.begin(8);
    Wire.onReceive(onReceive);
}
#endif

void
onReceive(int howMany) noexcept {

}

void
onRequest() noexcept {

}


void
setupTWI() noexcept {
    Serial.print(F("Configuring TWI..."));
    Wire.begin(8);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.println(F("DONE"));
}
void
setupSPI() noexcept {
    Serial.print(F("Configuring SPI..."));
    SPI.begin();
    Serial.println(F("DONE"));
}
void
setupSerial(bool displayBootScreen = true) noexcept {
    Serial.begin(115200);
    if (displayBootScreen) {
        Serial.println(F("i960 Simulator (C) 2022 and beyond Joshua Scoggins"));
        Serial.println(F("This software is open source software"));
        Serial.println(F("Base Platform: Arduino Mega2560"));
    }
}
void
bringUpSDCard() noexcept {
    Serial.println(F("Bringing up SD Card support"));
    while (!SD.begin(SDPin)) {
        Serial.println(F("SD Card not found, waiting a second and trying again!"));
        delay(1000);
    }
    Serial.println(F("Brought up SD Card support"));
}
void
setupEBI() noexcept {
    xmem::begin(true);
}
void
configureGPIOs() noexcept {
    Serial.print(F("Configuring GPIOs..."));
    pinMode(SDPin, OUTPUT);
    pinMode(PSRAMEnable, OUTPUT);
    digitalWrite(SDPin, HIGH);
    digitalWrite(PSRAMEnable, HIGH);
    Serial.println(F("DONE"));
}

template<int targetPin, bool performFullMemoryTest>
bool
setupPSRAM() noexcept {
    SPI.beginTransaction(SPISettings(F_CPU/2, MSBFIRST, SPI_MODE0));
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    digitalWrite(targetPin, LOW);
    SPI.transfer(0x66);
    digitalWrite(targetPin, HIGH);
    // test the first 64k instead of the full 8 megabytes
    constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
    for (uint32_t i = 0; i < endAddress; i += 4) {
        SplitWord32 container {i}, result {0};
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x02); // write
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[3]);
        digitalWrite(targetPin, HIGH);
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x03); // read 
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        result.bytes[0] = SPI.transfer(0);
        result.bytes[1] = SPI.transfer(0);
        result.bytes[2] = SPI.transfer(0);
        result.bytes[3] = SPI.transfer(0);
        digitalWrite(targetPin, HIGH);
        if (container != result) {
            return false;
        }
    }
    SPI.endTransaction();
    return true;
}
template<bool performFullMemoryTest>
void
bringUpPSRAM() noexcept {
    uint32_t memoryAmount = 0;
    auto addPSRAMAmount = [&memoryAmount]() {
        memoryAmount += (8ul * 1024ul * 1024ul);
    };
    if (setupPSRAM<PSRAMEnable, performFullMemoryTest>()) {
        addPSRAMAmount();
    }
    if (memoryAmount == 0) {
        Serial.println(F("NO MEMORY INSTALLED!"));
        while (true) {
            delay(1000);
        }
    } else {
        Serial.print(F("Detected "));
        Serial.print(memoryAmount);
        Serial.println(F(" bytes of memory!"));
    }
}

namespace {
    template<int targetPin>
    size_t
    psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x02);
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
        digitalWrite(targetPin, HIGH);
        return count;
    }

    template<int targetPin>
    size_t
    psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x03);
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
        digitalWrite(targetPin, HIGH);
        return count;
    }
}

size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite<PSRAMEnable>(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead<PSRAMEnable>(baseAddress, bytes, count);
}

template<typename T>
inline volatile T& memory(const size_t address) noexcept {
    return *reinterpret_cast<volatile T*>(address);
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
        xmem::setMemoryBank(0);
        auto BufferSize = 16384;
        auto* buffer = new byte[BufferSize]();
        for (uint32_t i = 0, j = 0; i < memoryImage.size(); i += BufferSize, ++j) {
            while (memoryImage.isBusy());
            SplitWord32 currentAddressLine(i);
            auto numRead = memoryImage.read(buffer, BufferSize);
            if (numRead < 0) {
                SD.errorHalt();
            }
            memoryWrite(currentAddressLine, buffer, numRead);
            if ((j % 16) == 0) {
                Serial.print(F("."));
            }
        }
        memoryImage.close();
        Serial.println();
        Serial.println(F("transfer complete!"));
        delete [] buffer;
    }
}
struct Cache {

};
struct CacheReference {
    void select() {
        xmem::setMemoryBank(index_);
    }
    void begin(byte index) noexcept {
        if (!initialized_) {
            initialized_ = true;
            index_ = index;
            select();
            ptr_ = new Cache();
        }
    }
    private:
        byte index_ = 0;
        bool initialized_ = false;
        Cache* ptr_ = nullptr;
};
CacheReference cacheBlocks_[64];
void
setupCache() noexcept {
    for (int i = 0, j = 16; i < 64; ++i, ++j) {
        cacheBlocks_[i].begin(j);
    }
    xmem::setMemoryBank(0);
}


void 
setup() {
    setupSerial();
    setupEBI();
    //setupCache();
    setupSPI();
    setupTWI();
    configureGPIOs();
    //bringUpSDCard();
    //bringUpPSRAM<false>();
    //installMemoryImage();
    // setup cache in the heap now!
}
void 
loop() {
    if (auto results = xmem::selfTest(); results.succeeded) {
        Serial.println(F("Memory Test Passed!"));
    } else {
        Serial.println(F("Memory Test Failed!"));
        Serial.print(F("Failing Address: 0x"));
        Serial.println(reinterpret_cast<uint16_t>(results.failedAddress), HEX);
        Serial.print(F("Failed Bank: 0x"));
        Serial.println(results.failedBank, HEX);
        Serial.print(F("Got Value: 0x")); Serial.println(static_cast<int>(results.gotValue), HEX);
        Serial.print(F("Expected Value: 0x")); Serial.println(static_cast<int>(results.expectedValue), HEX);
    }
    delay(1000);
}

namespace External328Bus {
    void setBank(uint8_t bank) noexcept {
        // set the upper
        digitalWrite(FakeA15, bank & 0b1 ? HIGH : LOW);
        PORTK = (bank >> 1) & 0b0111'1111;
        PORTF = 0;
    }
    void begin() noexcept {
        static constexpr int PinList[] {
            A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15
        };
        for (auto pin : PinList) {
            pinMode(pin, OUTPUT);
        }
        pinMode(FakeA15, OUTPUT);
        setBank(0);
    }
    void select() noexcept {
        digitalWrite(RealA15, HIGH);
    }
} // end namespace External328Bus
namespace InternalBus {
    void setBank(uint8_t bank) noexcept {
        digitalWrite(BANK0, bank & 0b0001 ? HIGH : LOW);
        digitalWrite(BANK1, bank & 0b0010 ? HIGH : LOW);
        digitalWrite(BANK2, bank & 0b0100 ? HIGH : LOW);
        digitalWrite(BANK3, bank & 0b1000 ? HIGH : LOW);
    }
    void begin() noexcept {
        pinMode(BANK0, OUTPUT);
        pinMode(BANK1, OUTPUT);
        pinMode(BANK2, OUTPUT);
        pinMode(BANK3, OUTPUT);
        setBank(0);
    }
    void select() noexcept {
        digitalWrite(RealA15, LOW);
    }
} // end namespace InternalBus
