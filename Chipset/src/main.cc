/*
i960SxChipset_Type103
Copyright (c) 2022-2023, Joshua Scoggins
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


#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
SdFs SD;
FsFile disk0;
constexpr auto TransferBufferSize = 16384;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;
constexpr bool PerformMemoryImageInstallation = true;
constexpr uintptr_t MemoryWindowBaseAddress = 0xC000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;
auto& DebugConsole = Serial;
auto& MemoryConnection = Serial2;
auto& ExternalPeripheralConnection = Serial1;
auto& ExternalPeripheralConnection2 = Serial3;
constexpr bool transactionDebugEnabled() noexcept {
#ifdef TRANSACTION_DEBUG
    return true;
#else
    return false;
#endif

}
constexpr bool EnableTransactionDebug = transactionDebugEnabled();


[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2210)]] inline volatile CH351 ControlSignals;
[[gnu::address(0x2218)]] inline volatile CH351 XBusBank;
[[gnu::address(0x8000)]] inline volatile uint8_t XBusWindow[64][256];
[[gnu::address(0x4000)]] inline volatile uint8_t IOXBusWindow[64][256];

// allocate 1024 bytes total
[[gnu::always_inline]] inline bool isBurstLast() noexcept { 
    return digitalRead<Pin::BLAST>() == LOW; 
}

template<auto ViewKind>
struct ViewIs final {
    using Type = decltype(ViewKind);
    static constexpr Type Value = ViewKind;
};
enum class DataPortInterfaceKind {
    None,
    IOExpander,
    AVRGPIO,
};

template<DataPortInterfaceKind kind>
struct DataPortInterface {
    static constexpr bool Valid = false;
    static constexpr auto InterfaceKind = kind;
    using Self = DataPortInterface<InterfaceKind>;
    DataPortInterface() = delete;
    ~DataPortInterface() = delete;
    DataPortInterface(const Self&) = delete;
    DataPortInterface(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
    static void configureInterface() noexcept {
        DataLinesInterface.view32.direction = 0;
        DataLinesInterface.view32.data = 0;
        getDirectionRegister<Port::DataLinesUpper>() = 0;
        getDirectionRegister<Port::DataLinesLower>() = 0;
    }
};

template<>
struct DataPortInterface<DataPortInterfaceKind::AVRGPIO> {
    static constexpr bool Valid = true;
    static constexpr auto InterfaceKind = DataPortInterfaceKind::AVRGPIO;
    using Self = DataPortInterface<InterfaceKind>;
    DataPortInterface() = delete;
    ~DataPortInterface() = delete;
    DataPortInterface(const Self&) = delete;
    DataPortInterface(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
    static void setLowerDataLinesDirection(uint8_t value) noexcept {
        getDirectionRegister<Port::DataLinesLower>() = value;
    }
    static void setUpperDataLinesDirection(uint8_t value) noexcept {
        getDirectionRegister<Port::DataLinesUpper>() = value;
    }
    static inline uint8_t getLowerDataByte() noexcept { return getInputRegister<Port::DataLinesLower>(); }
    static inline uint8_t getUpperDataByte() noexcept { return getInputRegister<Port::DataLinesUpper>(); }
    static inline void setLowerDataByte(uint8_t value) noexcept { getOutputRegister<Port::DataLinesLower>() = value; }
    static inline void setUpperDataByte(uint8_t value) noexcept { getOutputRegister<Port::DataLinesUpper>() = value; }
    static void configureInterface() noexcept {
        DataLinesInterface.view32.direction = 0;
        DataLinesInterface.view32.data = 0;
        getDirectionRegister<Port::DataLinesUpper>() = 0xff;
        getDirectionRegister<Port::DataLinesLower>() = 0xff;
    }
};

template<>
struct DataPortInterface<DataPortInterfaceKind::IOExpander> {
    static constexpr bool Valid = true;
    static constexpr auto InterfaceKind = DataPortInterfaceKind::IOExpander;
    using Self = DataPortInterface<InterfaceKind>;
    DataPortInterface() = delete;
    ~DataPortInterface() = delete;
    DataPortInterface(const Self&) = delete;
    DataPortInterface(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
    static void setLowerDataLinesDirection(uint8_t value) noexcept {
        DataLinesInterface.view8.direction[0] = value;
    }
    static void setUpperDataLinesDirection(uint8_t value) noexcept {
        DataLinesInterface.view8.direction[1] = value;
    }
    static inline uint8_t getLowerDataByte() noexcept { return DataLinesInterface.view8.data[0]; }
    static inline uint8_t getUpperDataByte() noexcept { return DataLinesInterface.view8.data[1]; }
    static inline void setLowerDataByte(uint8_t value) noexcept { DataLinesInterface.view8.data[0] = value; }
    static inline void setUpperDataByte(uint8_t value) noexcept { DataLinesInterface.view8.data[1] = value; }
    static void configureInterface() noexcept {
        DataLinesInterface.view32.direction = 0x0000'FFFF;
        DataLinesInterface.view32.data = 0;
        getDirectionRegister<Port::DataLinesUpper>() = 0;
        getDirectionRegister<Port::DataLinesLower>() = 0;
    }
};


template<DataPortInterfaceKind kind>
constexpr auto isValidKind_v = DataPortInterface<kind>::Valid;
constexpr auto getDataPortKind() noexcept { 
#ifdef DATA_INTERFACE_PORT_IS_CH351
    return DataPortInterfaceKind::IOExpander;
#else
    return DataPortInterfaceKind::AVRGPIO;
#endif
}
constexpr auto DataPortKind = getDataPortKind();
static_assert(isValidKind_v<DataPortKind>, "unsupported data interface kind provided");


struct DataInterface {
    using UnderlyingDataInterface = DataPortInterface<DataPortKind>;
    using Self = DataInterface;
    DataInterface() = delete;
    ~DataInterface() = delete;
    DataInterface(const Self&) = delete;
    DataInterface(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
    static void configureInterface() noexcept {
        UnderlyingDataInterface::configureInterface();
    }
    FORCE_INLINE static inline uint16_t getData() noexcept {
        auto result = makeWord(UnderlyingDataInterface::getUpperDataByte(), UnderlyingDataInterface::getLowerDataByte());
        if constexpr (EnableTransactionDebug) {
            DebugConsole.print(F("getData: 0x"));
            DebugConsole.println(result, HEX);
        }
        return result;
    }
    FORCE_INLINE static inline void setData(uint16_t value) noexcept {
        if constexpr (EnableTransactionDebug) {
            DebugConsole.printf(F("setData: 0x%x\n"), value);
        }
        UnderlyingDataInterface::setLowerDataByte(value);
        UnderlyingDataInterface::setUpperDataByte(static_cast<uint8_t>(value >> 8));
    }
    static void setLowerDataLinesDirection(uint8_t value) noexcept {
        UnderlyingDataInterface::setLowerDataLinesDirection(value);
    }
    static void setUpperDataLinesDirection(uint8_t value) noexcept {
        UnderlyingDataInterface::setUpperDataLinesDirection(value);
    }
    static uint8_t getLowerDataByte() noexcept { 
        auto result = UnderlyingDataInterface::getLowerDataByte();
        if constexpr (EnableTransactionDebug) {
            DebugConsole.print(F("getLowerDataByte: 0x"));
            DebugConsole.println(result, HEX);
        } 
        return result;
    }
    static uint8_t getUpperDataByte() noexcept { 
        auto result = UnderlyingDataInterface::getUpperDataByte();
        if constexpr (EnableTransactionDebug) {
            DebugConsole.print(F("getUpperDataByte: 0x"));
            DebugConsole.println(result, HEX);
        } 
        return result;
    }
    static void setLowerDataByte(uint8_t value) noexcept { 
        if constexpr (EnableTransactionDebug) {
            DebugConsole.print(F("setLowerDataByte: 0x"));
            DebugConsole.println(value, HEX);
        } 
        UnderlyingDataInterface::setLowerDataByte(value);
    }
    static void setUpperDataByte(uint8_t value) noexcept { 
        if constexpr (EnableTransactionDebug) {
            DebugConsole.print(F("setUpperDataByte: 0x"));
            DebugConsole.println(value, HEX);
        } 
        UnderlyingDataInterface::setUpperDataByte(value);
    }
};



constexpr uint8_t computeCycleWidth(uint8_t cycles) {
    return 0xFF - (cycles - 1);
}
constexpr uint8_t ReadyCycleWidth = computeCycleWidth(2);
constexpr uint8_t ReadyCycleStart = ReadyCycleWidth - 1;
[[gnu::always_inline]] inline void oneShotFire() noexcept {
    // fire a one-shot pulse. Use the most recently set width!
    TCNT2 = ReadyCycleStart;
}

template<uint8_t delayAmount = 4>
[[gnu::always_inline]]
inline void
signalReady() noexcept {
    // wait for the one shot to go on
    oneShotFire();
    if constexpr (delayAmount > 0) {
        // we need two extra cycles for the blocking nature of TCNT2
        insertCustomNopCount<delayAmount + 2>();
    }
}

using Register8 = volatile uint8_t&;
using Register16 = volatile uint16_t&;
template<int index>
struct TimerDescriptor { 
};
#define X(index) \
template<> \
struct TimerDescriptor< index > {  \
    static inline Register8 TCCRxA = TCCR ## index ## A ; \
    static inline Register8 TCCRxB = TCCR ## index ## B ; \
    static inline Register8 TCCRxC = TCCR ## index ## C ; \
    static inline Register16 TCNTx = TCNT ## index; \
    static inline Register16 ICRx = ICR ## index ; \
    static inline Register16 OCRxA = OCR ## index ## A ; \
    static inline Register16 OCRxB = OCR ## index ## B ; \
    static inline Register16 OCRxC = OCR ## index ## C ; \
}; \
constexpr TimerDescriptor< index > timer ## index 
X(1);
X(3);
X(4);
X(5);
#undef X

void 
putCPUInReset() noexcept {
    ControlSignals.ctl.data.reset = 0;
}
void 
pullCPUOutOfReset() noexcept {
    ControlSignals.ctl.data.reset = 1;
}

struct CacheLine {
    union {
        uint8_t reg;
        struct {
            uint8_t dirty : 1;
            uint8_t valid : 1;
        } bits;
    } flags;
    uint32_t key = 0;
    uint8_t line[16] = { 0 };
    constexpr bool valid() const noexcept {
        return flags.bits.valid;
    }
    constexpr bool dirty() const noexcept {
        return flags.bits.dirty;
    }
    constexpr bool needsCacheLineReplacement() const noexcept {
        return valid() && dirty();
    }
    constexpr bool matches(uint32_t address) const noexcept {
        return valid() && key == address;
    }
};
CacheLine onboardCache[256];
void
tryWriteCacheLine(CacheLine& line) {
    if (line.needsCacheLineReplacement()) {
        constexpr uint8_t size = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t[16]);
        MemoryConnection.write(size);
        MemoryConnection.write(1);
        MemoryConnection.write(reinterpret_cast<char*>(&line.key), sizeof(uint32_t));
        MemoryConnection.write(line.line, 16);
        // do the replacement
        line.flags.bits.dirty = false;
    }
}

void
drainStream(Stream& connection) {
    while (connection.available()) {
        (void)connection.read();
    }
}
void
readCacheLine(CacheLine& line, uint32_t alignedAddress) {
    //readFromSerialConnection(alignedAddress, line.line, 16);
    constexpr uint8_t size = sizeof(uint32_t) + sizeof(uint8_t);
    MemoryConnection.write(size);
    MemoryConnection.write(0);
    MemoryConnection.write(reinterpret_cast<char*>(&alignedAddress), sizeof(uint32_t));
    line.key = alignedAddress;
    line.flags.bits.valid = true;
    line.flags.bits.dirty = false;
    MemoryConnection.readBytes(line.line, 16);
    drainStream(MemoryConnection);
}

void
replaceCacheLine(CacheLine& line, uint32_t alignedAddress) {
    tryWriteCacheLine(line);
    readCacheLine(line, alignedAddress);
}
CacheLine& 
getCacheLine(uint32_t address) {
    uint32_t alignedAddress = address & 0xFFFF'FFF0;
    uint8_t index = static_cast<uint8_t>(alignedAddress >> 4);
    auto& line = onboardCache[index];
    if (!line.matches(alignedAddress)) {
        replaceCacheLine(line, alignedAddress);
    }
    return line;
}
template<bool isReadOperation>
uint8_t* 
getCacheLineContents(uint32_t address) {
    uint8_t offset = address & 0x0000'000F;
    auto& line = getCacheLine(address);
    if constexpr (!isReadOperation) {
        line.flags.bits.dirty = true;
    }
    return &line.line[offset];
}


template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 2, "Invalid index provided to setDataByte, must be less than 2");
    if constexpr (index < 2) {
        if constexpr (EnableTransactionDebug) {
            DebugConsole.printf(F("setDataByte<%d>(%x)\n"), index, value);
        }
        if constexpr (index == 0) {
            DataInterface::setLowerDataByte(value);
        } else {
            DataInterface::setUpperDataByte(value);
        }
    }
}

template<uint8_t index>
inline uint8_t getDataByte() noexcept {
    static_assert(index < 2, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 2) {
        if constexpr (EnableTransactionDebug) {
            DebugConsole.printf(F("getDataByte<%d>()\n"), index);
        }
        if constexpr (index == 0) {
            return DataInterface::getLowerDataByte();
        } else {
            return DataInterface::getUpperDataByte();
        }
    } else {
        return 0;
    }
}
enum class IBUSMemoryViewKind {
    SixteenK,
};

constexpr auto MemoryViewKind = IBUSMemoryViewKind::SixteenK;

template<IBUSMemoryViewKind kind>
struct MemoryInterfaceBackend {
    using Self = MemoryInterfaceBackend<kind>;
    MemoryInterfaceBackend() = delete;
    ~MemoryInterfaceBackend() = delete;
    MemoryInterfaceBackend(const Self&) = delete;
    MemoryInterfaceBackend(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
};


template<>
struct MemoryInterfaceBackend<IBUSMemoryViewKind::SixteenK> {
    using Self = MemoryInterfaceBackend<IBUSMemoryViewKind::SixteenK>;
    MemoryInterfaceBackend() = delete;
    ~MemoryInterfaceBackend() = delete;
    MemoryInterfaceBackend(const Self&) = delete;
    MemoryInterfaceBackend(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
private:
    static void doSingleReadOperation(DataRegister8 view) {
        auto lo = view[0];
        auto hi = view[1];
        if constexpr (EnableTransactionDebug) {
            auto value = makeWord(hi, lo);
            DebugConsole.printf(F("doReadOperation: 0x%x\n"), value);
        }
        DataInterface::setLowerDataByte(lo);
        DataInterface::setUpperDataByte(hi);
    }
    static DataRegister8 computeTransactionAddress() {
        return memoryPointer<uint8_t>((AddressLinesInterface.view16.data[0]) | MemoryWindowBaseAddress);
    }
public:
    static void configure() noexcept {
        //getDirectionRegister<Port::IBUS_Bank>() = 0;
    }
    template<auto BufferSize>
    static void installMemoryImage(File& theFirmware) {
        auto* theBuffer = memoryPointer<uint8_t>(MemoryWindowBaseAddress);
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            // just modify the bank as we go along
            AddressLinesInterface.view32.data = address;
            theFirmware.read(const_cast<uint8_t*>(theBuffer), BufferSize);
            Serial.print(F("."));
        }
        if constexpr (EnableTransactionDebug) {
            AddressLinesInterface.view32.data = 0;
            for (int i = 0; i < 32; ++i) {
                DebugConsole.printf(F("0x%x: 0x%x\n"), i, theBuffer[i]);
            }
        }
    }
private:
    template<uint8_t baseIndex>
    FORCE_INLINE 
    inline
    static 
    void doReadSingular(DataRegister8 view) noexcept {
        auto lo = view[baseIndex + 0];
        auto hi = view[baseIndex + 1];
        DataInterface::setLowerDataByte(lo);
        DataInterface::setUpperDataByte(hi);
    }
public:
    FORCE_INLINE inline static void doReadOperation(DataRegister8 view) noexcept {
        doReadSingular<0>(view);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady<0>();
        doReadSingular<2>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<4>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<6>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<8>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<10>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<12>(view);
        if (isBurstLast()) { 
            signalReady<0>();
            return;
        } 
        signalReady<0>();
        doReadSingular<14>(view);
        signalReady<0>();
    }
    template<bool isReadOperation>
    FORCE_INLINE inline static void doOperation(DataRegister8 view) noexcept {
        if constexpr (isReadOperation) {
            doReadOperation(view);
        } else {
            doWriteOperation(view);
        }
    }
    template<bool isReadOperation>
    FORCE_INLINE inline static void doOperation() noexcept {
        doOperation<isReadOperation>(computeTransactionAddress());
    }
    FORCE_INLINE inline static void doWriteOperation(DataRegister8 view) noexcept {
        auto body = [&view]() {
            // we can pull the data off the bus and 
            // request the next set of data from the i960 while we are stashing
            // the current data
            auto lo = getDataByte<0>();
            auto hi = getDataByte<1>();
            // we can drop the wait states because the store process will be
            // taking place while the ready signal is being propagated
            signalReady<0>();
            // we do not need to check the enable signals because we already
            // know that we are going to be continuing execution of this
            // transaction. Thus we can ignore them and just do stores.
            //
            // If we are burst last then we only have to check BE1 because we
            // "flow" into the end of the transaction.
            view[0] = lo;
            view[1] = hi;
            view += 2;
        };
        auto whenDone = [&view]() {
            view[0] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                view[1] = getDataByte<1>();
            }
            signalReady<0>();
        };
        if (isBurstLast()) {
            if (digitalRead<Pin::BE0>() == LOW) {
                view[0] = getDataByte<0>();
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                view[1] = getDataByte<1>();
            }
            signalReady<0>();
        } else {
            if (digitalRead<Pin::BE0>() == LOW) {
                view[0] = getDataByte<0>();
            }
            auto hi = getDataByte<1>();
            signalReady<0>();
            view[1] = hi;
            view += 2;
            insertCustomNopCount<2>(); // insert delay to make sure that
                                       // isBurstLast is updated, replace with
                                       // more operations if found
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            if (isBurstLast()) {
                whenDone();
                return;
            }
            body();
            whenDone();
        }
    }
};

using MemoryInterface = MemoryInterfaceBackend<MemoryViewKind>;
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady()
template<bool isReadOperation>
FORCE_INLINE
inline
void
doNothing() {
    if constexpr (isReadOperation) {
        DataInterface::setData(0);
    }
    while (!isBurstLast()) {
        signalReady();
    }
    signalReady<0>();
}

template<bool isReadOperation>
FORCE_INLINE
inline 
void
doCoreIO() noexcept {
    switch (AddressLinesInterface.view8.data[0]) {
        case 0: { 
                    if constexpr (isReadOperation) { 
                        DataInterface::setData(static_cast<uint16_t>(F_CPU));
                    } 
                    I960_Signal_Switch;
                } 
        case 2: { 
                    if constexpr (isReadOperation) { 
                        DataInterface::setData(static_cast<uint16_t>((F_CPU) >> 16));
                    } 
                    I960_Signal_Switch;
                } 
        case 4: { 
                    if constexpr (isReadOperation) { 
                        DataInterface::setData(static_cast<uint16_t>(F_CPU / 2));
                    } 
                    I960_Signal_Switch;
                } 
        case 6: { 
                    if constexpr (isReadOperation) { 
                        DataInterface::setData(static_cast<uint16_t>((F_CPU / 2) >> 16));
                    } 
                    I960_Signal_Switch;
                } 
        case 8: { 
                    /* Serial RW connection */
                    if constexpr (isReadOperation) { 
                        DataInterface::setData(Serial.read());
                    } else { 
                        // no need to check this out just ignore the byte
                        // enable lines
                        // needs to be separate operations to make sure that
                        // the compiler doesn't do dumb things when optimizing!
                        volatile uint8_t value = getDataByte<0>();
                        Serial.write(value);
                    } 
                    I960_Signal_Switch;
                } 
        case 10: {
                     if constexpr (isReadOperation) { 
                         DataInterface::setData(0);
                     } 
                     I960_Signal_Switch;
                 } 
        case 12: { 
                     if constexpr (isReadOperation) { 
                         DataInterface::setData(0);
                     } else { 
                         Serial.flush();
                     }
                     I960_Signal_Switch;
                 }
        case 14: {
                     /* nothing to do on writes but do update the data port
                      * on reads */ 
                     if constexpr (isReadOperation) { 
                         DataInterface::setData(0);
                     } 
                 }
                 break;
#define X(obj, offset) \
        case offset + 0: { \
                             /* TCCRnA and TCCRnB */ \
                             if constexpr (isReadOperation) { \
                                 setDataByte<0>(obj.TCCRxA);\
                                 setDataByte<1>(obj.TCCRxB);\
                             } else { \
                                 if (digitalRead<Pin::BE0>() == LOW) { \
                                     obj.TCCRxA = getDataByte<0>();\
                                 } \
                                 if (digitalRead<Pin::BE1>() == LOW) { \
                                     obj.TCCRxB = getDataByte<1>();\
                                 } \
                             } \
                             I960_Signal_Switch;\
                         } \
        case offset + 2: { \
                             /* TCCRnC and Reserved (ignore that) */ \
                             if constexpr (isReadOperation) { \
                                 setDataByte<0>(obj.TCCRxC);\
                                 setDataByte<1>(0); \
                             } else { \
                                 if (digitalRead<Pin::BE0>() == LOW) { \
                                     obj.TCCRxC = getDataByte<0>();\
                                 } \
                             } \
                             I960_Signal_Switch;\
                         } \
        case offset + 4: { \
                             /* TCNTn should only be accessible if you do a full 16-bit
                              * write 
                              */ \
                             if constexpr (isReadOperation) { \
                                 noInterrupts(); \
                                     auto tmp = obj.TCNTx; \
                                     interrupts();  \
                                     DataInterface::setData(tmp); \
                             } else {  \
                                 if (digitalRead<Pin::BE0>() == LOW &&  \
                                         digitalRead<Pin::BE1>() == LOW) {  \
                                     auto value = DataInterface::getData();  \
                                         noInterrupts();  \
                                         obj.TCNTx = value; \
                                         interrupts();  \
                                 }  \
                             }  \
                             I960_Signal_Switch; \
                         }  \
        case offset + 6: {  \
                             /* ICRn should only be accessible if you do a full 16-bit 
                              * write
                              */  \
                             if constexpr (isReadOperation) { \
                                 noInterrupts(); \
                                     auto tmp = obj.ICRx;\
                                     interrupts(); \
                                     DataInterface::setData(tmp); \
                             } else { \
                                 if (digitalRead<Pin::BE0>() == LOW &&  \
                                         digitalRead<Pin::BE1>() == LOW) { \
                                     auto value = DataInterface::getData(); \
                                         noInterrupts(); \
                                         obj.ICRx = value;\
                                         interrupts(); \
                                 } \
                             } \
                             I960_Signal_Switch;\
                         } \
        case offset + 8: { \
                             /* OCRnA should only be accessible if you do a full 16-bit write */ \
                                 if constexpr (isReadOperation) { \
                                     noInterrupts(); \
                                         auto tmp = obj.OCRxA;\
                                         interrupts(); \
                                         DataInterface::setData(tmp); \
                                 } else { \
                                     if (digitalRead<Pin::BE0>() == LOW &&  \
                                             digitalRead<Pin::BE1>() == LOW) { \
                                         auto value = DataInterface::getData(); \
                                             noInterrupts(); \
                                             obj.OCRxA = value;\
                                             interrupts(); \
                                     } \
                                 } \
                             I960_Signal_Switch;\
                         } \
        case offset + 10: {\
                              /* OCRnB */ \
                                  if constexpr (isReadOperation) { \
                                      noInterrupts(); \
                                          auto tmp = obj.OCRxB;\
                                          interrupts(); \
                                          DataInterface::setData(tmp); \
                                  } else { \
                                      if (digitalRead<Pin::BE0>() == LOW &&  \
                                              digitalRead<Pin::BE1>() == LOW) { \
                                          auto value = DataInterface::getData(); \
                                              noInterrupts(); \
                                              obj.OCRxB = value; \
                                              interrupts(); \
                                      } \
                                  } \
                              I960_Signal_Switch;\
                          } \
        case offset + 12: { \
                              /* OCRnC */ \
                                  if constexpr (isReadOperation) { \
                                      noInterrupts(); \
                                          auto tmp = obj.OCRxC; \
                                          interrupts(); \
                                          DataInterface::setData(tmp); \
                                  } else { \
                                      if (digitalRead<Pin::BE0>() == LOW && \
                                              digitalRead<Pin::BE1>() == LOW) { \
                                          auto value = DataInterface::getData(); \
                                              noInterrupts(); \
                                              obj.OCRxC = value;\
                                              interrupts(); \
                                      }\
                                  } \
                              I960_Signal_Switch;\
                          } \
        case offset + 14: { \
                              /* nothing to do on writes but do update the data port
                               * on reads */ \
                              if constexpr (isReadOperation) { \
                                  DataInterface::setData(0); \
                              } \
                              break;\
                          }  
#ifdef TCCR1A
                          X(timer1, 0x10);
#endif
#ifdef TCCR4A
                          X(timer4, 0x20);
#endif
#ifdef TCCR5A
                          X(timer5, 0x30);
#endif
#undef X
        case 0x40: {
                       if constexpr (isReadOperation) {
                           volatile uint32_t result = millis();
                           DataInterface::setData(static_cast<uint16_t>(result));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(result >> 16));
                           I960_Signal_Switch;
                       } else {
                           I960_Signal_Switch;
                           I960_Signal_Switch;
                       }
                   }
        case 0x44: {
                       if constexpr (isReadOperation) {
                           volatile uint32_t result = micros();
                           DataInterface::setData(static_cast<uint16_t>(result));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(result >> 16));
                           I960_Signal_Switch;
                       } else {
                           I960_Signal_Switch;
                           I960_Signal_Switch;
                       }
                   }
        case 0x48: {
                       if constexpr (isReadOperation) {
                           DataInterface::setData(0);
                       }
                       I960_Signal_Switch;
                       I960_Signal_Switch;
                   }
        case 0x4c: {
                       if constexpr (isReadOperation) {
                           DataInterface::setData(0);
                       }
                       I960_Signal_Switch;
                       I960_Signal_Switch;
                   }
                   break;
                   // disk0 is special and is supposed to be as available as
                   // possible
#if 0
                   struct Disk0View {
                       uint64_t capacity; // in bytes
                       uint64_t position; // in bytes
                       uint16_t available; // 
                       int16_t rw; 
                   };
#endif
        case 0x50: {
                       // capacity register
                       if constexpr (isReadOperation) {
                           auto capacity = disk0.size();
                           DataInterface::setData(capacity);
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(capacity >> 16));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(capacity >> 32));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(capacity >> 48));
                           I960_Signal_Switch;
                       } else {
                           I960_Signal_Switch;
                           I960_Signal_Switch;
                           I960_Signal_Switch;
                           I960_Signal_Switch;
                       }
                   }
        case 0x58: {
                       // setup the seeking register
                       if constexpr (isReadOperation) {
                           auto pos = disk0.curPosition();
                           DataInterface::setData(pos);
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(pos >> 16));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(pos >> 32));
                           I960_Signal_Switch;
                           DataInterface::setData(static_cast<uint16_t>(pos >> 48));
                           I960_Signal_Switch;
                       } else {
                           uint64_t a = static_cast<uint64_t>(DataInterface::getData());
                           I960_Signal_Switch;
                           uint64_t b = static_cast<uint64_t>(DataInterface::getData()) << 16;
                           I960_Signal_Switch;
                           uint64_t c = static_cast<uint64_t>(DataInterface::getData()) << 32;
                           I960_Signal_Switch;
                           uint64_t d = static_cast<uint64_t>(DataInterface::getData()) << 48;
                           uint64_t newPosition = a | b | c | d;
                           if (newPosition >= disk0.size()) {
                               newPosition %= disk0.size();
                           }
                           disk0.seek(newPosition);
                           I960_Signal_Switch;
                       }

                   }
        case 0x60: {
                       // available lookup
                       if constexpr (isReadOperation) {
                           DataInterface::setData(disk0.isOpen() ? 0xFFFF : 0);
                       } 
                       I960_Signal_Switch;
                   }
        case 0x62: {
                       // rw port
                       if constexpr (isReadOperation) {
                           int16_t result = disk0.read();
                           DataInterface::setData(result);
                       } else {
                           // write a value out
                           auto value = static_cast<uint8_t>(DataInterface::getData());
                           (void)disk0.write(value);
                       }
                       I960_Signal_Switch;
                   }
        default:
                   doNothing<isReadOperation>();
                   return;
    } 
    signalReady<0>(); 
}

template<bool isReadOperation>
FORCE_INLINE
inline
void 
doIO() noexcept { 
#if 0
        switch (AddressLinesInterface.view8.data[1]) {
            case 0x00:
                doCoreIO<isReadOperation>();
                break;
#define Block1K(offset)  \
                X((offset + 0x00)); \
                X((offset + 0x01)); \
                X((offset + 0x02)); \
                X((offset + 0x03))
#define Block2K(offset)  \
                Block1K((offset + 0x00)); \
                Block1K((offset + 0x04)) 
#define Block4K(offset)  \
                Block2K((offset + 0x00)); \
                Block2K((offset + 0x08))
#define Block16K(offset) \
                Block4K((offset + 0x00)); \
                Block4K((offset + 0x10)); \
                Block4K((offset + 0x20)); \
                Block4K((offset + 0x30))
#define X(id) case (0x40 + id) : MemoryInterface::doOperation<isReadOperation>(&IOXBusWindow[id][AddressLinesInterface.view8.data[0]]); break
                //Block16K(0);
#undef X
#define X(id) case (0x80 + id) : MemoryInterface::doOperation<isReadOperation>(&XBusWindow[id][AddressLinesInterface.view8.data[0]]); break
                //Block16K(0);
#undef X
#undef Block16K
#undef Block4K
#undef Block2K
#undef Block1K
            default:
                doNothing<isReadOperation>();
                break;
        } 
#else
        doCoreIO<isReadOperation>();
#endif
}
template<bool isReadOperation>
inline
void
doExternalCommunication() noexcept {
    uint32_t address = AddressLinesInterface.view32.data;
    auto* line = getCacheLineContents<isReadOperation>(address);
    MemoryInterface::doOperation<isReadOperation>(line);
}
template<bool isReadOperation, bool externalCommunicationSupported>
FORCE_INLINE
inline
void
doIOOperation() noexcept {
    switch (AddressLinesInterface.view8.data[3]) {
        case 0x00:
            // we don't need to worry about the upper 16-bits of the bus like we
            // used to. In this improved design, there is no need to keep track of
            // where we are starting. Instead, we can easily just do the check as
            // needed
            MemoryInterface::doOperation<isReadOperation>();
            break;
        case 0xFE:
            doIO<isReadOperation>();
            break;
        default:
            if constexpr (externalCommunicationSupported) {
                doExternalCommunication<isReadOperation>();
            } else {
                doNothing<isReadOperation>();
            }
            break;
    }
}
#undef I960_Signal_Switch

template<uint32_t maxFileSize = MaximumBootImageFileSize, auto BufferSize = TransferBufferSize>
[[gnu::noinline]]
void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = maxFileSize;
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    static constexpr auto filePath = "prog.bin";
    // look for firmware.bin and open it readonly
    if (auto theFirmware = SD.open(filePath, FILE_READ); !theFirmware) {
        Serial.printf(F("Could not open %s for reading!"), filePath);
        while (true) {
            delay(1000);
        }
    } else if (theFirmware.size() > MaximumFileSize) {
        Serial.println(F("The firmware image is too large to fit in sram!"));
        while (true) {
            delay(1000);
        }
    } else {
        Serial.println(F("TRANSFERRING!!"));
        MemoryInterface::installMemoryImage<BufferSize>(theFirmware);
        Serial.println(F("DONE!"));
        theFirmware.close();
    }
    // okay so now end reading from the SD Card
    //SD.end();
    SPI.endTransaction();
}

CPUKind 
getInstalledCPUKind() noexcept { 
    //return static_cast<CPUKind>((getInputRegister<Port::CTL960>() >> 5) & 0b111);
    return static_cast<CPUKind>(ControlSignals.ctl.data.cfg);
}
void 
setupReadySignal() noexcept {
    pinMode<Pin::ONE_SHOT_READY>(OUTPUT);
    // taken from https://github.com/bigjosh/TimerShot/blob/master/TimerShot.ino and adapted to work with a mega2560
    TCCR2B = 0; // disable the counter completely
    TCNT2 = 0x00; // start counting at the bottom
    OCR2A = 0; // set TOP to 0. This will keep us from counting because the
               // counter just keeps resetting back to zero.

               // we break out of this by manually setting TCNT higher than 0,
               // in which case it will count all the way up to MAX and then
               // overflow back to 0 and get locked up again.
    OCR2B = ReadyCycleWidth; // we want two cycles at 20MHz, this also makes new OCR
                             // values get loaded from the buffer on every clock
                             // cycle
                        
    // we want to generate a falling signal so do Clear OC2B on Compare Match
    // and set OC2B at BOTTOM
    TCCR2A = _BV(COM2B1) | _BV(WGM20) | _BV(WGM21); // waveform generation
                                                    // using FastPWM mode
    TCCR2B = _BV(WGM22) | _BV(CS20); // enable the counter and select fastPWM mode 7
}
void
setupCLK1() noexcept {
    // configure PE3 to be CLK1
    pinMode<Pin::CLK1>(OUTPUT);
    digitalWrite<Pin::CLK1, LOW>();
    timer3.TCCRxA = 0b01'00'00'00;
    timer3.TCCRxB = 0b00'0'01'001;
}


volatile bool foundExternalMemoryConnection = false;
constexpr uint8_t outputBuffer[2] { 1, 2, };
void
setupMemoryConnection() noexcept {
    Serial.print(F("Setting up memory cache"));
    MemoryConnection.begin(115'200);
    // clear the cache
    for (auto & a : onboardCache) {
        a.key = 0;
        a.flags.bits.valid = false;
        a.flags.bits.dirty = false;
        for (int i = 0; i < 16; ++i) {
            a.line[i] = 0;
        }
    }
    Serial.println(F("DONE"));
    Serial.print(F("Looking to see if memory connection is available"));
    uint8_t resultantBuffer[2];
    for (int i = 0; i < 32; ++i) {
        Serial.print(F("."));
        MemoryConnection.write(outputBuffer, 2);
        auto count = MemoryConnection.readBytes(resultantBuffer, 2);
        if (count == 2) {
            if (resultantBuffer[0] == 0x1 && resultantBuffer[1] == 0x55) {
                foundExternalMemoryConnection = true;
                break;
            }
        }
    }
    Serial.println(F("DONE"));
    if (!foundExternalMemoryConnection) {
        Serial.println(F("No External Memory Connection Found!"));
    } else {
        Serial.println(F("External Memory Connection Found!"));
    }
}


void
setup() {
    int32_t seed = 0;
#define X(value) seed += analogRead(value) 
    X(A0); X(A1); X(A2); X(A3);
    X(A4); X(A5); X(A6); X(A7);
    X(A8); X(A9); X(A10); X(A11);
    X(A12); X(A13); X(A14); X(A15);
#undef X
    randomSeed(seed);
    Serial.begin(115200);
    setupMemoryConnection();
    ExternalPeripheralConnection.begin(500'000);
    ExternalPeripheralConnection2.begin(500'000);
    SPI.begin();
    // power down the ADC
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate ADC
    setupCLK1();
    setupReadySignal();

    
    // enable interrupt pin output
    pinMode<Pin::INT0_960_>(OUTPUT);
    digitalWrite<Pin::INT0_960_, HIGH>();
    // setup the IBUS bank
    pinMode(Pin::IsMemorySpaceOperation, INPUT);
    pinMode(Pin::BE0, INPUT);
    pinMode(Pin::BE1, INPUT);
    pinMode(Pin::BLAST, INPUT);

    // set these up ahead of time

    // setup bank capture to read in address lines
    pinMode(Pin::LED, OUTPUT);
    digitalWrite<Pin::LED, LOW>();
    pinMode(Pin::NewTransaction, INPUT);
    pinMode(Pin::ExternalMemoryOperation, INPUT);
    pinMode(Pin::WriteTransaction, INPUT);
    // setup the EBI
    XMCRB=0b0'0000'000;
    XMCRA=0b1'010'01'01;  
    // we divide the sector limits so that it 0x2200-0x3FFF and 0x4000-0xFFFF
    // the single cycle wait state is necessary even with the AHC573s
    AddressLinesInterface.view32.direction = 0xFFFF'FFFE;
    AddressLinesInterface.view32.data = 0;
    DataInterface::configureInterface();
    MemoryInterface::configure();
    ControlSignals.view32.direction = 0b10000000'11111110'00000000'00010001;
    ControlSignals.view32.data =      0b00000000'11111110'00000000'00000000;
    putCPUInReset();
    Serial.println(F("i960 Chipset"));
    Serial.println(F("(C) 2019-2023 Joshua Scoggins"));
    Serial.print(F("Detected i960 CPU Kind: "));
    auto cpuNotSupported = []() {
        Serial.println(F("Target CPU is not supported by this firmware!"));
        while(true);
    };
    switch (getInstalledCPUKind()) {
        case CPUKind::Sx:
            Serial.println(F("Sx"));
            break;
        case CPUKind::Kx:
            Serial.println(F("Kx"));
            cpuNotSupported();
            break;
        case CPUKind::Jx:
            Serial.println(F("Jx"));
            cpuNotSupported();
            break;
        case CPUKind::Hx:
            Serial.println(F("Hx"));
            cpuNotSupported();
            break;
        case CPUKind::Cx:
            Serial.println(F("Cx"));
            cpuNotSupported();
            break;
        default:
            Serial.println(F("Unknown"));
            cpuNotSupported();
            break;
    }
    Serial.println(F("Looking for an SD Card!"));
    {
        while (!SD.begin(static_cast<int>(Pin::SD_EN))) {
            Serial.println(F("NO SD CARD!"));
            delay(1000);
        }
    }
    Serial.println(F("SD CARD FOUND!"));
    // find firmware.bin and install it into the 512k block of memory
    if constexpr (PerformMemoryImageInstallation) {
        installMemoryImage();
    } else {
        delay(1000);
    }
    disk0 = SD.open("disk0.dsk", O_RDWR);
    if (!disk0) {
        Serial.println(F("Could not open disk0.dsk"));
        Serial.println(F("No hard drive will be available"));
    }
    // put the address line capture io expander back into input mode
    AddressLinesInterface.view32.direction = 0;
    // attach interrupts
    EICRB = 0b1010'1010; // falling edge on the upper four interrupts
                         // don't enable the interrupt handler
    pullCPUOutOfReset();
}
template<bool allowMemoryConnection>
[[noreturn]]
void
executionBody() {
    while (true) {
        //read
        while (true) {
            loop_until_bit_is_set(EIFR, INTF4);
            if (bit_is_set(EIFR, INTF5)) {
                DataInterface::setLowerDataLinesDirection(0);
                DataInterface::setUpperDataLinesDirection(0);
                EIFR = 0b0111'0000;
                doIOOperation<false, allowMemoryConnection>();
                break;
            }
            EIFR = 0b0111'0000;
            doIOOperation<true, allowMemoryConnection>();
        }
        // write
        while (true) {
            loop_until_bit_is_set(EIFR, INTF4);
            if (bit_is_clear(EIFR, INTF5)) {
                DataInterface::setLowerDataLinesDirection(0xFF);
                DataInterface::setUpperDataLinesDirection(0xFF);
                EIFR = 0b0111'0000;
                doIOOperation<true, allowMemoryConnection>();
                break;
            }
            EIFR = 0b0111'0000;
            doIOOperation<false, allowMemoryConnection>();
        }
    }
}
void 
loop() {
    if (foundExternalMemoryConnection) {
        executionBody<true>();
    } else {
        executionBody<false>();
    }
}



