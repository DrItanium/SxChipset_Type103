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

#ifndef SXCHIPSET_TYPE103_TYPES_H__
#define SXCHIPSET_TYPE103_TYPES_H__
#include <Arduino.h>
#include <SdFat.h>
#include "Detect.h"



template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];
extern SdFat SD;
constexpr auto OffsetSize = 4; // 16-byte line
constexpr auto getTagSize() noexcept {
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
    return 7;
#elif defined(TYPE200_BOARD)
    return 6;
#else
# error "Unknown tag size!"
    return 0;
#endif
}
constexpr auto getNumberOfSets() noexcept {
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
    return 128;
#elif defined(TYPE200_BOARD)
    return 64;
#else
# error "Unknown tag count!"
    return 0;
#endif

}
constexpr auto TagSize = getTagSize(); // 8192 bytes divided into 16-byte
                                       // lines with 4 lines per set
                                       // (4-way)
constexpr auto KeySize = 32 - (OffsetSize + TagSize);
enum class IOGroup : byte{
    Peripherals,
    Undefined,
};
static_assert(static_cast<byte>(IOGroup::Undefined) <= 16, "Too many IO groups defined!");
constexpr IOGroup getGroup(uint8_t value) noexcept {
    switch (static_cast<IOGroup>(value & 0b1111)) {
        case IOGroup::Peripherals:
            return static_cast<IOGroup>(value);
        default:
            return IOGroup::Undefined;
    }
}

enum class EnableStyle : byte {
    Full16 = 0b00,
    Upper8 = 0b01,
    Lower8 = 0b10,
    Undefined = 0b11,
};

union Word8 {
    explicit Word8(uint8_t value = 0) noexcept : value_(value) { }

    uint8_t value_;
    struct {
        uint8_t be : 2;
        uint8_t blast : 1;
        uint8_t den : 1;
        uint8_t fail : 1;
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
        uint8_t dataInt : 1;
#else
        uint8_t wr : 1;
#endif
    } channel0;
    /**
     * @brief Address bits [A1, A7] + W/~{R} in place of A0
     */
    struct {
        uint8_t a0 : 1;
        uint8_t addr : 7;
    } lowestAddr;
    /**
     * @brief Address bits [A8, A15]
     */
    uint8_t lowerAddr;
    /**
     * @brief Address bits [A16, A23]
     */
    uint8_t upperAddr;
    struct {
        uint8_t valid_ : 1;
        uint8_t dirty_ : 1;
    } lineFlags;
    struct {
        uint8_t rst : 1;
        uint8_t hold : 1;
        uint8_t rest : 6;
    } xioPortADir;
    [[nodiscard]] constexpr bool isReadOperation() const noexcept { 
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
        return lowestAddr.a0 == 0; 
#else
        return channel0.wr == 0;
#endif
    }
    [[nodiscard]] constexpr bool isWriteOperation() const noexcept { 
        return !isReadOperation();
    }
    [[nodiscard]] constexpr EnableStyle getByteEnable() const noexcept { return static_cast<EnableStyle>(channel0.be); }
    [[nodiscard]] constexpr bool dataInterruptTriggered() const noexcept { 
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
        return channel0.dataInt == 0; 
#else
        return true;
#endif
    }
    [[nodiscard]] constexpr auto getAddressBits1_7() const noexcept { 
        return lowestAddr.addr;
    }
    [[nodiscard]] constexpr auto getAddressBits0_7() const noexcept { 
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
        return getAddressBits1_7() << 1; 
#else
        return value_;
#endif
    }
    [[nodiscard]] constexpr auto getAddressBits8_15() const noexcept { return lowerAddr; }
    [[nodiscard]] constexpr auto getAddressBits16_23() const noexcept { return upperAddr; }
    [[nodiscard]] constexpr auto isBurstLast() const noexcept { return channel0.blast == 0; }

    [[nodiscard]] constexpr auto lineIsValid() const noexcept { return lineFlags.valid_; }
    [[nodiscard]] constexpr auto lineIsDirty() const noexcept { return lineFlags.dirty_; }
    void clear() noexcept {
        value_ = 0;
    }
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const Word8& other) const noexcept { return value_ == other.value_; }
    [[nodiscard]] constexpr bool operator!=(const Word8& other) const noexcept { return value_ != other.value_; }
    [[nodiscard]] constexpr bool operator<(const Word8& other) const noexcept { return value_ < other.value_; }
    [[nodiscard]] constexpr bool operator<=(const Word8& other) const noexcept { return value_ <= other.value_; }
    [[nodiscard]] constexpr bool operator>(const Word8& other) const noexcept { return value_ > other.value_; }
    [[nodiscard]] constexpr bool operator>=(const Word8& other) const noexcept { return value_ >= other.value_; }
    [[nodiscard]] constexpr bool operator==(uint8_t other) const noexcept { return value_ == other; }
    [[nodiscard]] constexpr bool operator!=(uint8_t other) const noexcept { return value_ != other; }
    [[nodiscard]] constexpr bool operator<(uint8_t other) const noexcept { return value_ < other; }
    [[nodiscard]] constexpr bool operator<=(uint8_t other) const noexcept { return value_ <= other; }
    [[nodiscard]] constexpr bool operator>(uint8_t other) const noexcept { return value_ > other; }
    [[nodiscard]] constexpr bool operator>=(uint8_t other) const noexcept { return value_ >= other; }
};
using Channel0Value = Word8;
using Channel1Value = Word8;
using Channel2Value = Word8;
using Channel3Value = Word8;

union SplitWord16 {
    uint16_t full;
    ElementContainer<uint16_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint16_t, uint8_t>; }
    constexpr SplitWord16() : full(0) { }
    constexpr explicit SplitWord16(uint16_t value) : full(value) { }
    constexpr explicit SplitWord16(uint8_t a, uint8_t b) : bytes{a, b} { }
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return full; }
    void setWholeValue(uint16_t value) noexcept { full = value; }
    [[nodiscard]] constexpr bool operator==(const SplitWord16& other) const noexcept { return full == other.full; }
    [[nodiscard]] constexpr bool operator!=(const SplitWord16& other) const noexcept { return full != other.full; }
    [[nodiscard]] constexpr bool operator==(uint16_t other) const noexcept { return full == other; }
    [[nodiscard]] constexpr bool operator!=(uint16_t other) const noexcept { return full != other; }
    [[nodiscard]] constexpr bool operator<(const SplitWord16& other) const noexcept { return full < other.full; }
    [[nodiscard]] constexpr bool operator<=(const SplitWord16& other) const noexcept { return full <= other.full; }
    [[nodiscard]] constexpr bool operator>(const SplitWord16& other) const noexcept { return full > other.full; }
    [[nodiscard]] constexpr bool operator>=(const SplitWord16& other) const noexcept { return full >= other.full; }
};


union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    ElementContainer<uint32_t, SplitWord16> word16;
    constexpr SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    struct {
        uint32_t a0 : 1;
        uint32_t offset : (OffsetSize - 1);
        uint32_t rest : (32 - OffsetSize);
    } address;
    struct {
        uint8_t subfunction;
        uint8_t function;
        uint8_t device;
        uint8_t group : 4;
        uint8_t req : 4;
    } ioRequestAddress;
    struct {
        uint32_t offset : 23;
        uint32_t targetDevice : 3;
        uint32_t rest : 6;
    } psramAddress;
    /**
     * @brief Describes a virtual 32-bit address with 4k pages, does not take 
     */
    struct {
        /**
         * @brief The offset into the 4k page
         */
        uint32_t offset : 12;
        /**
         * @brief The page number inside of the given region
         */
        uint32_t pageNumber : 18;
        /**
         * @brief the region that this address points to
         */
        uint32_t region : 2;
    } virtualAddress;
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return full; }
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    [[nodiscard]] constexpr bool isIOInstruction() const noexcept { return ioRequestAddress.req == 0xF; }
    [[nodiscard]] constexpr IOGroup getIOGroup() const noexcept { return getGroup(ioRequestAddress.group); }
    [[nodiscard]] constexpr uint8_t getIODeviceCode() const noexcept { return ioRequestAddress.device; }
    [[nodiscard]] constexpr uint8_t getIOFunctionCode() const noexcept { return ioRequestAddress.function; }
    template<typename E>
    [[nodiscard]] constexpr E getIODevice() const noexcept { return static_cast<E>(getIODeviceCode()); }
    template<typename E>
    [[nodiscard]] constexpr E getIOFunction() const noexcept { return static_cast<E>(getIOFunctionCode()); }
    [[nodiscard]] constexpr auto getAddressOffset() const noexcept { return address.offset; }
    [[nodiscard]] constexpr bool operator==(const SplitWord32& other) const noexcept { return full == other.full; }
    [[nodiscard]] constexpr bool operator!=(const SplitWord32& other) const noexcept { return full != other.full; }
    [[nodiscard]] constexpr bool operator<(const SplitWord32& other) const noexcept { return full < other.full; }
    [[nodiscard]] constexpr bool operator<=(const SplitWord32& other) const noexcept { return full <= other.full; }
    [[nodiscard]] constexpr bool operator>(const SplitWord32& other) const noexcept { return full > other.full; }
    [[nodiscard]] constexpr bool operator>=(const SplitWord32& other) const noexcept { return full >= other.full; }
    [[nodiscard]] constexpr auto retrieveHalf(byte offset) const noexcept { return halves[offset & 0b1]; }
    void assignHalf(byte offset, uint16_t value) noexcept { halves[offset & 0b1] = value; }
    void clear() noexcept { full = 0; }
};
static_assert(sizeof(SplitWord32) == sizeof(uint32_t), "SplitWord32 must be the exact same size as a 32-bit unsigned int");


class TransactionInterface {
    public:
        virtual ~TransactionInterface() = default;
        virtual void startTransaction(const SplitWord32& addr) noexcept { };
        virtual uint16_t read(const Channel0Value& m0) const noexcept = 0;
        virtual void write(const Channel0Value& m0, uint16_t value) noexcept = 0;
        virtual void next() noexcept { }
        virtual void endTransaction() noexcept { };
};
class AddressTracker {
public:
    void recordAddress(const SplitWord32& addr) noexcept {
        address_ = addr;
        offset_ = addr.getAddressOffset();
    }
    void advanceOffset() noexcept {
        ++offset_;
    }
    void clear() noexcept {
        address_.clear();
        offset_ = 0;
    }
    [[nodiscard]] constexpr auto getAddress() const noexcept  { return address_; }
    [[nodiscard]] constexpr auto getOffset() const noexcept { return offset_; }
private:
    SplitWord32 address_{0};
    byte offset_ = 0;
};
/**
 * @brief Communication primitive for talking to the i960 or other devices,
 */
class OperationHandler : public TransactionInterface, public AddressTracker {
    public:
        virtual ~OperationHandler() = default;
        void startTransaction(const SplitWord32& addr) noexcept override {
            recordAddress(addr);
        }
        void next() noexcept override {
            advanceOffset();
        }
        void endTransaction() noexcept override { }
};

/**
 * @brief Fallback/through handler implementation
 */
class NullHandler final {
public:
    void startTransaction(const SplitWord32&) noexcept { }
    uint16_t read(const Channel0Value&) const noexcept { return 0; }
    void write(const Channel0Value&, uint16_t ) noexcept { }
    void endTransaction() noexcept { }
    void next() noexcept { }
};

inline NullHandler& getNullHandler() noexcept  {
    static NullHandler temp;
    return temp;
}

#ifndef _BV
#define _BV(value) (1 << value)
#endif
constexpr auto pow2(uint8_t value) noexcept {
    return _BV(value);
}
static_assert(pow2(0) == 1);
static_assert(pow2(1) == 2);
static_assert(pow2(2) == 4);
static_assert(pow2(3) == 8);
static_assert(pow2(4) == 16);
static_assert(pow2(5) == 32);
static_assert(pow2(6) == 64);
static_assert(pow2(7) == 128);
static_assert(pow2(8) == 256);
static_assert(pow2(9) == 512);
static_assert(pow2(10) == 1024);


#endif //SXCHIPSET_TYPE103_TYPES_H__
