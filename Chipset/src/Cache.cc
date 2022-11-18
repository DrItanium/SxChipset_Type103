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
#include "SPI.h"
#include "Types.h"
#include "Pinout.h"

namespace {
    DataCache cache;
    File ramFile;
    size_t
    swapMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        ramFile.seekSet(baseAddress.full);
        return ramFile.write(bytes, count);
    }
    size_t
    swapMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        ramFile.seekSet(baseAddress.full);
        return ramFile.read(bytes, count);
    }
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

    void
    setupRAMFile() noexcept {
        if (!ramFile.open("ram.bin", FILE_WRITE)) {
            Serial.println(F("Could not open ram.bin!"));
            while (true) {
                delay(1000);
            }
        }
    }

}

DataCache& 
getCache() noexcept {
    return cache;
}
size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    //return psramMemoryWrite(baseAddress, bytes, count);
    return swapMemoryWrite(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    //return psramMemoryRead(baseAddress, bytes, count);
    return swapMemoryRead(baseAddress, bytes, count);
}
void 
setupCache() noexcept {
    setupRAMFile();
    cache.begin();
    cache.clear();
}
