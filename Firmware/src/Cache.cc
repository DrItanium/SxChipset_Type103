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
#include "Types.h"

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
namespace {
    MultipartCache cache;
    File ramFile;
}

SplitCache& 
getCache() noexcept {
    return cache;
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
