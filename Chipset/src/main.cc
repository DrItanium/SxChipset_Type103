/*
i960SxChipset_Type103
Copyright (c) 2022-2024, Joshua Scoggins
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

extern "C" [[noreturn]] void ExecutionBody();
extern "C" [[gnu::used]] void doIOReadOperation();
extern "C" [[gnu::used]] void doIOWriteOperation();
using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
SdFs SD;
FsFile disk0;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;
constexpr bool PerformMemoryImageInstallation = true;
constexpr bool EnableRegularHoldSignal = false;

[[gnu::address(0x2300)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2308)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2310)]] inline volatile CH351 ControlSignals;
inline 
void
holdBus() noexcept {
    if (bit_is_clear(GPIOR0, 0)) {
        digitalWrite<Pin::HOLD, HIGH>();
        bitSet(GPIOR0, 0);
    }
}

inline 
void
releaseBus() noexcept {
    if (bit_is_set(GPIOR0, 0)) {
        digitalWrite<Pin::HOLD, LOW>();
        bitClear(GPIOR0, 0);
    }
}

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


template<DataPortInterfaceKind kind>
constexpr auto isValidKind_v = DataPortInterface<kind>::Valid;
constexpr auto getDataPortKind() noexcept { 
    return DataPortInterfaceKind::AVRGPIO;
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
    static inline uint16_t getData() noexcept {
        return makeWord(UnderlyingDataInterface::getUpperDataByte(), UnderlyingDataInterface::getLowerDataByte());
    }
    static inline void setData(uint16_t value) noexcept {
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
        return UnderlyingDataInterface::getLowerDataByte();
    }
    static uint8_t getUpperDataByte() noexcept { 
        return UnderlyingDataInterface::getUpperDataByte();
    }
    static void setLowerDataByte(uint8_t value) noexcept { 
        UnderlyingDataInterface::setLowerDataByte(value);
    }
    static void setUpperDataByte(uint8_t value) noexcept { 
        UnderlyingDataInterface::setUpperDataByte(value);
    }
};

enum class ReadySignalKind : uint8_t {
    TimerBased,
    SoftwareGPIO,
};
constexpr bool valid(ReadySignalKind kind) noexcept {
    switch (kind) {
        case ReadySignalKind::TimerBased:
        case ReadySignalKind::SoftwareGPIO:
            return true;
        default:
            return false;
    }
}
template<ReadySignalKind kind>
struct UseReadySignalKind final {

};
constexpr auto TargetReadySignal = ReadySignalKind::TimerBased;
static_assert(valid(TargetReadySignal), "Invalid READY signal handler specified!");
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
[[gnu::always_inline]] inline void signalReady(UseReadySignalKind<ReadySignalKind::TimerBased>) noexcept {
    // wait for the one shot to go on
    oneShotFire();
    if constexpr (delayAmount > 0) {
        // it takes two cycles avr for the ready signal to actually signal
        // ready. The i960 needs to detect the signal and act accordingly.
        // so it will be 6 cycles every single time between the time we start
        // the trigger and when we have the next transaction starting
        insertCustomNopCount<delayAmount + 2>();
    }
}

template<uint8_t delayAmount = 4>
[[gnu::always_inline]] 
inline 
void 
signalReady(UseReadySignalKind<ReadySignalKind::SoftwareGPIO>) noexcept {
    toggle<Pin::ReadyDirect>();
    if constexpr (delayAmount > 0) {
        // it takes two cycles avr for the ready signal to actually signal
        // ready. The i960 needs to detect the signal and act accordingly.
        // so it will be 6 cycles every single time between the time we start
        // the trigger and when we have the next transaction starting
        insertCustomNopCount<delayAmount + 2>();
    }
}
template<uint8_t delayAmount = 4>
[[gnu::always_inline]]
inline void
signalReady() noexcept {
    signalReady<delayAmount>(UseReadySignalKind<TargetReadySignal>{});
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
#undef X

void 
putCPUInReset() noexcept {
    digitalWrite<Pin::Reset, LOW>();
}
void 
pullCPUOutOfReset() noexcept {
    digitalWrite<Pin::Reset, HIGH>();
}

template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 2, "Invalid index provided to setDataByte, must be less than 2");
    if constexpr (index < 2) {
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
    EightBit,
};

constexpr auto MemoryViewKind = IBUSMemoryViewKind::EightBit;

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
struct MemoryInterfaceBackend<IBUSMemoryViewKind::EightBit> {
    using Self = MemoryInterfaceBackend<IBUSMemoryViewKind::EightBit>;
    MemoryInterfaceBackend() = delete;
    ~MemoryInterfaceBackend() = delete;
    MemoryInterfaceBackend(const Self&) = delete;
    MemoryInterfaceBackend(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
private:
    static constexpr uint8_t MemoryWindowUpperHalf = 0x22;
    static constexpr uintptr_t MemoryWindowBaseAddress = (static_cast<uint16_t>(MemoryWindowUpperHalf) << 8);
    static constexpr auto TransferBufferSize = 256;
public:
    static void configure() noexcept {
    }
    [[gnu::noinline]]
    static void installMemoryImage(File& theFirmware) {
        volatile auto* theBuffer = memoryPointer<uint8_t>(MemoryWindowBaseAddress);
        int counter = 0;
        for (uint32_t address = 0; address < theFirmware.size(); address += TransferBufferSize, ++counter) {
            // just modify the bank as we go along
            AddressLinesInterface.view32.data = address;
            theFirmware.read(const_cast<uint8_t*>(theBuffer), TransferBufferSize);
            if ((counter % 32) == 0) {
                Serial.print('.');
            }
        }
#if 0
        AddressLinesInterface.view32.data = 0;
        volatile auto* theBuffer2 = memoryPointer<uint32_t>(MemoryWindowBaseAddress);
        for (auto i = 0u; i < (TransferBufferSize / sizeof(uint32_t)); ++i) {
            Serial.printf(F("0x%x: 0x%lx\n"), i, theBuffer2[i]);
        }
#endif
    }
};

using MemoryInterface = MemoryInterfaceBackend<MemoryViewKind>;
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady()
template<bool isReadOperation>
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
inline 
void
doLegacyIO() noexcept {
    switch ( getInputRegister<Port::AddressLinesLowest>()) {
    
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
class DataBlock {
public:
    void clear() {
        for (int i = 0 ; i< 256; ++i) {
            block[i] = 0;
        }
    }
    template<bool isReadOperation>
    void processRequest() {
        DataRegister8 blockStart = &block[getInputRegister<Port::AddressLinesLowest>()];
        if constexpr (isReadOperation) {
            processReadRequest(blockStart);
        } else {
            processWriteRequest(blockStart);
        }
    }
    [[nodiscard]] uint8_t& operator[](uint8_t index) noexcept {
        return block[index];
    }

private:
    void 
    processReadRequest(DataRegister8 ptr) noexcept {
        DataRegister16 ptr16 = reinterpret_cast<DataRegister16>(ptr);
        DataInterface::setData(ptr16[0]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[1]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[2]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[3]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[4]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[5]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[6]);
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        DataInterface::setData(ptr16[7]);
        signalReady();
    }
    void 
    processWriteRequest(DataRegister8 ptr) noexcept {
        if (digitalRead<Pin::BE0>() == LOW) {
            ptr[0] = getDataByte<0>();
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            ptr[1] = getDataByte<1>();
        }
        if (isBurstLast()) {
            signalReady<0>();
            return;
        }
        signalReady();
        // flow into this
        ptr[2] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[3] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[3] = getDataByte<1>();
        signalReady();
        // flow into this
        ptr[4] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[5] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[5] = getDataByte<1>();
        signalReady();
        // we flow into this
        ptr[6] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[7] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[7] = getDataByte<1>();
        signalReady();
        // we flow into this
        ptr[8] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[9] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[9] = getDataByte<1>();
        signalReady();
        // we flow into this
        ptr[10] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[11] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[11] = getDataByte<1>();
        signalReady();
        // we flow into this
        ptr[12] = getDataByte<0>();
        if (isBurstLast()) {
            if (digitalRead<Pin::BE1>() == LOW) {
                ptr[13] = getDataByte<1>();
            }
            signalReady<0>();
            return;
        }
        ptr[13] = getDataByte<1>();
        signalReady();
        ptr[14] = getDataByte<0>();
        if (digitalRead<Pin::BE1>() == LOW) {
            ptr[15] = getDataByte<1>();
        }
        signalReady();
    }
private:
    uint8_t block[256];
};

constexpr auto NumberOfBlocks = 4;
DataBlock blocks[NumberOfBlocks];
void setupDataBlocks() {
    for (int i = 0; i < NumberOfBlocks; ++i) {
        for (int j = 0; j < 256; ++j) {
            blocks[i][j] = random();
        }
        blocks[i].clear();
    }
}
template<bool isReadOperation>
void 
doIO() noexcept { 
    switch (AddressLinesInterface.view8.data[1]) {
        case 0x00:
            doLegacyIO<isReadOperation>();
            break;
        case 0x01: // this is a 256byte data mapping
            blocks[0].processRequest<isReadOperation>();
            break;
        case 0x02: // this is a 256byte data mapping
            blocks[1].processRequest<isReadOperation>();
            break;
        case 0x03: // this is a 256byte data mapping
            blocks[2].processRequest<isReadOperation>();
            break;
        case 0x04: // this is a 256byte data mapping
            blocks[3].processRequest<isReadOperation>();
            break;
        default:
            doLegacyIO<isReadOperation>();
            break;
    }
}

void 
doIOReadOperation() {
    doIO<true>();
}
void 
doIOWriteOperation() {
    doIO<false>();
}
#undef I960_Signal_Switch

template<uint32_t maxFileSize = MaximumBootImageFileSize>
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
        MemoryInterface::installMemoryImage(theFirmware);
        Serial.println(F("DONE!"));
        theFirmware.close();
    }
    // okay so now end reading from the SD Card
    SPI.endTransaction();
}

void 
setupReadySignal() noexcept {
    if constexpr (TargetReadySignal == ReadySignalKind::TimerBased) {
        pinMode<Pin::ReadyDirect>(INPUT);
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
    } else if (TargetReadySignal == ReadySignalKind::SoftwareGPIO) {
        pinMode<Pin::ONE_SHOT_READY>(INPUT);
        pinMode<Pin::ReadyDirect>(OUTPUT);
        digitalWrite<Pin::ReadyDirect, HIGH>();
    } else {
        Serial.println(F("NO READY SIGNAL SPECIFIED... HALTING"));
        while (true) {
            delay(1000);
        }
    }
}
void
setupCLK5Mhz() noexcept {
    pinMode<Pin::CLK5Mhz>(OUTPUT);
    digitalWrite<Pin::CLK5Mhz, LOW>();
    OCR4A = 1;
    TCNT4 = 0;
    TCCR4A = 0b01'00'00'00;
    TCCR4B = 0b00'0'01'001;
}
void
setupCLK10Mhz() noexcept {
    // configure PE3 to be CLK1
    pinMode<Pin::CLK1>(OUTPUT);
    digitalWrite<Pin::CLK1, LOW>();
    OCR3A = 0;
    TCNT3 = 0;
    TCCR3A = 0b01'00'00'00;
    TCCR3B = 0b00'0'01'001;
}
void 
setupHoldTimer() noexcept {
    TCCR5A = 0b00'00'00'01; // Normal timer mode
    TCNT5 = 0; // zero out the timer
    OCR5A = 0xFFFF;
    OCR5B = 0x8000;
    OCR5C = 0xC000;
    if constexpr (EnableRegularHoldSignal) {
        TIMSK5 = 0b00'0'0'000'1; // overflow interrupt enable plus the comparison
                                 // interrupts
    } else {
        TIMSK5 = 0;
    }
    TCCR5B = 0b0'0'0'00'011; // divide by 8 prescalar
}
ISR(TIMER5_OVF_vect) {
    if constexpr (EnableRegularHoldSignal) {
        if (digitalRead<Pin::Lock>() == HIGH) {
            holdBus();
        }
    }
}

ISR(INT3_vect) {
    if constexpr (EnableRegularHoldSignal) {
        {
            AddressLinesInterface.view32.direction = 0xFFFF'FFFE;
            AddressLinesInterface.view32.data = 0;
            {
                // code goes here
                // if we get here then it is a legit operation
                // at the end we want to revert all of our direction changes as well
            }
            AddressLinesInterface.view32.direction = 0;
        }
        releaseBus();
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
    SPI.begin();
    // power down the ADC
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate ADC
    setupCLK10Mhz();
    setupReadySignal();
    setupHoldTimer();
    
    // enable interrupt pin output
    pinMode<Pin::INT0_960_>(OUTPUT);
    digitalWrite<Pin::INT0_960_, HIGH>();
    // setup the IBUS bank
    pinMode(Pin::IsMemorySpaceOperation, INPUT);
    pinMode(Pin::BE0, INPUT);
    pinMode(Pin::BE1, INPUT);
    pinMode(Pin::BLAST, INPUT);
    pinMode(Pin::Reset, OUTPUT);
    digitalWrite<Pin::Reset, LOW>();
    // set these up ahead of time
    pinMode(Pin::Lock, INPUT);
    pinMode(Pin::HOLD, OUTPUT);
    digitalWrite<Pin::HOLD, LOW>();
    pinMode(Pin::HLDA, INPUT);
    // setup bank capture to read in address lines
    pinMode(Pin::LED, OUTPUT);
    digitalWrite<Pin::LED, LOW>();
    pinMode(Pin::NewTransaction, INPUT);
    pinMode(Pin::ExternalMemoryOperation, INPUT);
    pinMode(Pin::WriteTransaction, INPUT);
    pinMode(Pin::DEN, INPUT);
    pinMode(Pin::WR, INPUT);
    // setup the EBI
    XMCRB=0b0'0000'000;
    XMCRA=0b1'010'01'01;  
    //XMCRA=0b1'010'00'00;
    // we divide the sector limits so that it 0x2200-0x3FFF and 0x4000-0xFFFF
    // the single cycle wait state is necessary even with the AHC573s
    AddressLinesInterface.view32.direction = 0xFFFF'FFFE;
    AddressLinesInterface.view32.data = 0;
    getDirectionRegister<Port::AddressLinesLowest>() = 0;
    getOutputRegister<Port::AddressLinesLowest>() = 0;
    DataInterface::configureInterface();
    MemoryInterface::configure();
    ControlSignals.view32.direction = 0b10000000'11111110'00000000'00000000;
    ControlSignals.view32.data =      0b00000000'11111110'00000000'00000000;
    GPIOR0 = 0;
    putCPUInReset();
    setupDataBlocks();
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
    EICRA = 0b11'00'00'00; // rising edge on INT0 but no trigger at this
                           // point, trigger on high
    EICRB = 0b0000'0010; // falling edge on INT4 only
    pullCPUOutOfReset();
    if constexpr (EnableRegularHoldSignal) {
        bitSet(EIMSK, INT3); // only activate after we have pulled the cpu out of
                             // reset
    }
}
void 
loop() {
    // instead of controlling execution directly, we should be using a slight
    // time slicing design to make sure that we have the ability to process
    // packets from external chips connected over serial.
    ExecutionBody();
}

