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

constexpr auto TransferBufferSize = 16384;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;
constexpr bool PerformMemoryImageInstallation = true;
constexpr uintptr_t MemoryWindowBaseAddress = 0x4000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;
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
[[gnu::address(0x4000)]] inline volatile uint8_t memoryPort8[2];

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
    static uint8_t getLowerDataByte() noexcept { return getInputRegister<Port::DataLinesLower>(); }
    static uint8_t getUpperDataByte() noexcept { return getInputRegister<Port::DataLinesUpper>(); }
    static void setLowerDataByte(uint8_t value) noexcept { getOutputRegister<Port::DataLinesLower>() = value; }
    static void setUpperDataByte(uint8_t value) noexcept { getOutputRegister<Port::DataLinesUpper>() = value; }
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
    static uint8_t getLowerDataByte() noexcept { 
        return DataLinesInterface.view8.data[0];
    }
    static uint8_t getUpperDataByte() noexcept { 
        return DataLinesInterface.view8.data[1];
    }
    static void setLowerDataByte(uint8_t value) noexcept { 
        DataLinesInterface.view8.data[0] = value; 
    }
    static void setUpperDataByte(uint8_t value) noexcept { 
        DataLinesInterface.view8.data[1] = value; 
    }
    static void configureInterface() noexcept {
        DataLinesInterface.view32.direction = 0x0000'FFFF;
        DataLinesInterface.view32.data = 0;
        getDirectionRegister<Port::DataLinesUpper>() = 0;
        getDirectionRegister<Port::DataLinesLower>() = 0;
    }
};


template<DataPortInterfaceKind kind>
constexpr auto isValidKind_v = DataPortInterface<kind>::Valid;

constexpr auto DataPortKind = DataPortInterfaceKind::AVRGPIO;
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
    static uint16_t getData() noexcept {
        auto result = makeWord(UnderlyingDataInterface::getUpperDataByte(), UnderlyingDataInterface::getLowerDataByte());
        if constexpr (EnableTransactionDebug) {
            Serial.print(F("getData: 0x"));
            Serial.println(result, HEX);
        }
        return result;
    }
    static void setData(uint16_t value) noexcept {
        if constexpr (EnableTransactionDebug) {
            Serial.printf(F("setData: 0x%x\n"), value);
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
            Serial.print(F("getLowerDataByte: 0x"));
            Serial.println(result, HEX);
        } 
        return result;
    }
    static uint8_t getUpperDataByte() noexcept { 
        auto result = UnderlyingDataInterface::getUpperDataByte();
        if constexpr (EnableTransactionDebug) {
            Serial.print(F("getUpperDataByte: 0x"));
            Serial.println(result, HEX);
        } 
        return result;
    }
    static void setLowerDataByte(uint8_t value) noexcept { 
        if constexpr (EnableTransactionDebug) {
            Serial.print(F("setLowerDataByte: 0x"));
            Serial.println(value, HEX);
        } 
        UnderlyingDataInterface::setLowerDataByte(value);
    }
    static void setUpperDataByte(uint8_t value) noexcept { 
        if constexpr (EnableTransactionDebug) {
            Serial.print(F("setUpperDataByte: 0x"));
            Serial.println(value, HEX);
        } 
        UnderlyingDataInterface::setUpperDataByte(value);
    }
};



constexpr uint8_t computeCycleWidth(uint8_t cycles) {
    return 0xFF - (cycles - 1);
}
constexpr uint8_t ReadyCycleWidth = computeCycleWidth(2);
constexpr uint8_t ReadyCycleStart = ReadyCycleWidth - 1;
[[gnu::always_inline]] inline void oneShotFire() {
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



template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 2, "Invalid index provided to setDataByte, must be less than 2");
    if constexpr (index < 2) {
        if constexpr (EnableTransactionDebug) {
            Serial.printf(F("setDataByte<%d>(%x)\n"), index, value);
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
            Serial.printf(F("getDataByte<%d>()\n"), index);
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
FORCE_INLINE
inline
void idleTransaction() noexcept {
    while (!isBurstLast()) {
        signalReady();
    }
    signalReady<0>();
}
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady()

template<bool isReadOperation>
FORCE_INLINE 
inline 
void doIO() noexcept { 
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
                        Serial.write(static_cast<uint8_t>(getDataByte<0>()));
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
                          X(timer4, 0x30);
#endif
#ifdef TCCR5A
                          X(timer5, 0x40);
#endif
#undef X
        case 0x50: {
                    if constexpr (isReadOperation) {
                        auto result = millis();
                        DataInterface::setData(static_cast<uint16_t>(result));
                        I960_Signal_Switch;
                        DataInterface::setData(static_cast<uint16_t>(result >> 16));
                        I960_Signal_Switch;
                    } else {
                        I960_Signal_Switch;
                        I960_Signal_Switch;
                    }
                   }
        case 0x54: {
                    if constexpr (isReadOperation) {
                        auto result = micros();
                        DataInterface::setData(static_cast<uint16_t>(result));
                        I960_Signal_Switch;
                        DataInterface::setData(static_cast<uint16_t>(result >> 16));
                        I960_Signal_Switch;
                    } else {
                        I960_Signal_Switch;
                        I960_Signal_Switch;
                    }
                   }
        case 0x58: {
                       if constexpr (isReadOperation) {
                           DataInterface::setData(0);
                       }
                       I960_Signal_Switch;
                       I960_Signal_Switch;
                   }
        case 0x5c: {
                       if constexpr (isReadOperation) {
                           DataInterface::setData(0);
                       }
                       I960_Signal_Switch;
                       I960_Signal_Switch;
                   }
                   break;
        default:
                          if constexpr (isReadOperation) {
                              DataInterface::setData(0);
                          }
                          idleTransaction();
                          return;
    } 
    signalReady<0>(); 
}
#undef I960_Signal_Switch
enum class IBUSMemoryViewKind {
    SixteenK,
    TwoByte,
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
struct MemoryInterfaceBackend<IBUSMemoryViewKind::TwoByte> {
    using Self = MemoryInterfaceBackend<IBUSMemoryViewKind::TwoByte>;
    MemoryInterfaceBackend() = delete;
    ~MemoryInterfaceBackend() = delete;
    MemoryInterfaceBackend(const Self&) = delete;
    MemoryInterfaceBackend(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
private:
    static void doSingleReadOperation() {
        auto lo = memoryPort8[0];
        auto hi = memoryPort8[1];
        if constexpr (EnableTransactionDebug) {
            auto value = makeWord(hi, lo);
            Serial.printf(F("doReadOperation: 0x%x\n"), value);
        }
        DataInterface::setLowerDataByte(lo);
        DataInterface::setUpperDataByte(hi);
    }
public:
    template<auto BufferSize>
    static void installMemoryImage(File& theFirmware) {
        uint32_t transferDots = 0;
        auto* theBuffer = memoryPort8;
        for (uint32_t address = 0; address < theFirmware.size(); address += 2, ++transferDots) {
            // just modify the bank as we go along
            AddressLinesInterface.view32.data = address;
            theFirmware.read(const_cast<uint8_t*>(theBuffer), 2);
            if ((transferDots % (BufferSize / 2)) == 0) {
                Serial.print(F("."));
            }
        }
    }
    static void doReadOperation() noexcept {
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation();
        signalReady();
Read_Done:
        doSingleReadOperation();
        signalReady<0>();
    }
    static void doWriteOperation() noexcept {
        if (digitalRead<Pin::BE0>() == LOW) {
            memoryPort8[0] = getDataByte<0>();
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            memoryPort8[1] = getDataByte<1>();
        }
        if (isBurstLast()) { 
            goto Write_SignalDone; 
        } 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
        if (isBurstLast()) {
            goto Write_Done;
        }
        memoryPort8[0] = getDataByte<0>(); 
        memoryPort8[1] = getDataByte<1>(); 
        signalReady();
Write_Done:
        memoryPort8[0] = getDataByte<0>();
        if (digitalRead<Pin::BE1>() == LOW) {
            memoryPort8[1] = getDataByte<1>();
        }
Write_SignalDone:
        signalReady<0>();
    }
    static void configure() noexcept {
    }
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
            Serial.printf(F("doReadOperation: 0x%x\n"), value);
        }
        DataInterface::setLowerDataByte(lo);
        DataInterface::setUpperDataByte(hi);
    }
    static DataRegister8 computeTransactionAddress() {
        return memoryPointer<uint8_t>((AddressLinesInterface.view16.data[0] & 0x3FFF) | 0x4000);
    }
public:
    static void configure() noexcept {
        getDirectionRegister<Port::IBUS_Bank>() = 0;
    }
    template<auto BufferSize>
    static void installMemoryImage(File& theFirmware) {
        auto* theBuffer = memoryPointer<uint8_t>(0x4000);
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            // just modify the bank as we go along
            AddressLinesInterface.view32.data = address;
            theFirmware.read(const_cast<uint8_t*>(theBuffer), BufferSize);
            Serial.print(F("."));
        }
    }
    static void doReadOperation() noexcept {
        DataRegister8 view = computeTransactionAddress();
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
        if (isBurstLast()) { 
            goto Read_Done; 
        } 
        doSingleReadOperation(view);
        signalReady();
        view += 2;
Read_Done:
        doSingleReadOperation(view);
        signalReady<0>();
    }
    static void doWriteOperation() noexcept {
        DataRegister8 view = computeTransactionAddress();
        if (digitalRead<Pin::BE0>() == LOW) {
            view[0] = getDataByte<0>();
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            view[1] = getDataByte<1>();
        }
        if (isBurstLast()) { 
            goto Write_SignalDone; 
        } 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
        if (isBurstLast()) {
            goto Write_Done;
        }
        view[0] = getDataByte<0>(); 
        view[1] = getDataByte<1>(); 
        signalReady();
        view += 2;
Write_Done:
        view[0] = getDataByte<0>();
        if (digitalRead<Pin::BE1>() == LOW) {
            view[1] = getDataByte<1>();
        }
Write_SignalDone:
        signalReady<0>();
    }
};

using MemoryInterface = MemoryInterfaceBackend<MemoryViewKind>;

template<bool isReadOperation>
FORCE_INLINE
inline
void
doIOOperation() noexcept {
    if (digitalRead<Pin::IsMemorySpaceOperation>()) {
        // we don't need to worry about the upper 16-bits of the bus like we
        // used to. In this improved design, there is no need to keep track of
        // where we are starting. Instead, we can easily just do the check as
        // needed
        if constexpr (isReadOperation) {
            if constexpr (EnableTransactionDebug) {
                Serial.println(F("read memory operation"));
            }
            MemoryInterface::doReadOperation();
        } else {
            if constexpr (EnableTransactionDebug) {
                Serial.println(F("write memory operation"));
            }
            MemoryInterface::doWriteOperation();
        }
    } else {
        doIO<isReadOperation>();
    }
}

template<uint32_t maxFileSize = MaximumBootImageFileSize, auto BufferSize = TransferBufferSize>
[[gnu::noinline]]
void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = maxFileSize;
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    SdFs SD;
    Serial.println(F("Looking for an SD Card!"));
    {
        while (!SD.begin(static_cast<int>(Pin::SD_EN))) {
            Serial.println(F("NO SD CARD!"));
            delay(1000);
        }
    }
    Serial.println(F("SD CARD FOUND!"));
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
    SD.end();
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
    // power down the ADC and USART3
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate ADC
    PRR1 = 0b00000'100; // deactivate USART3
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
    pinMode(Pin::ReadTransaction, INPUT);
    pinMode(Pin::WriteTransaction, INPUT);
    // setup the EBI
    XMCRB=0b0'0000'000;
    XMCRA=0b1'010'01'01;  
    // we divide the sector limits so that it 0x2200-0x3FFF and 0x4000-0xFFFF
    // the single cycle wait state is necessary even with the AHC573s
    AddressLinesInterface.view32.direction = 0xFFFF'FFFE;
    AddressLinesInterface.view32.data = 0;
    DataInterface::configureInterface();
    ControlSignals.view32.direction = 0b10000000'11111110'00000000'00010001;
    ControlSignals.view32.data =      0b00000000'11111110'00000000'00000000;
    putCPUInReset();
    MemoryInterface::configure();
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
    // find firmware.bin and install it into the 512k block of memory
    if constexpr (PerformMemoryImageInstallation) {
        installMemoryImage();
    } else {
        delay(1000);
    }
    // put the address line capture io expander back into input mode
    AddressLinesInterface.view32.direction = 0;
    // attach interrupts
    EICRB = 0b1010'1010; // falling edge on the upper four interrupts
                         // don't enable the interrupt handler
    pullCPUOutOfReset();
}
void 
loop() {
    // this microcontroller is not responsible for signalling ready manually
    // in this method. Instead, an external piece of hardware known as "Timing
    // Circuit" in the Intel manuals handles all external timing. It is drawn
    // as a box which takes in the different enable signals and generates the
    // ready signal sent to the i960 based off of the inputs provided. It has
    // eluded me for a very long time. I finally realized what it acutually is,
    // a counter that is configured around delay times for non intelligent
    // devices (flash, sram, dram, etc) and a multiplexer to allow intelligent
    // devices to control the ready signal as well.
    //
    // In my design, this mysterious circuit is a GAL22V10 which takes in a
    // 10MHz clock signal and provides a 4-bit counter and multiplexer to
    // accelerate ready signal propagation and also tell the mega 2560 when to
    // respond to the i960. The ready signal is rerouted from direct connection
    // of the i960 to the GAL22V10. Right now, I have to waste an extra pin on
    // the 2560 for this but my plan is to connect this directly to the 22V10
    // instead. 
    //
    // I also have connected the DEN and space detect pins to this 22V10 to
    // complete the package. I am not using the least significant counter bit
    // and instead the next bit up to allow for proper delays. The IO pin is
    // used to activate the mega2560 instead of the DEN pin directly. This is
    // currently a separate pin. In the future, I will be making this
    // infinitely more flexible by rerouting the DEN and READY pins into
    // external hardware that allows me to directly control the design again if
    // I so desire. 
    //
    // This change can also allow me to use more than one microcontroller for
    // IO devices if I so desire :D. 
    //
    // This version of the handler method assumes that you are within the
    // 16-megabyte window in the i960's memory space where this microcontroller
    // lives. So we wait for the GAL22V10 to tell me it is good to go to
    // continue!
ReadOperationStart:
    // read operation
    // wait until DEN goes low
    loop_until_bit_is_set(EIFR, INTF4);
    if (bit_is_set(EIFR, INTF5)) {
        // change direction to output since we are doing write -> read
        DataInterface::setLowerDataLinesDirection(0);
        DataInterface::setUpperDataLinesDirection(0);
        // then jump into the write loop
        goto WriteOperationBypass;
    } 
ReadOperationBypass:
    EIFR = 0b0111'0000;
    if constexpr (EnableTransactionDebug) {
        Serial.print(F("R 0x")); 
        Serial.println(AddressLinesInterface.view32.data, HEX);
    }
    doIOOperation<true>();
    goto ReadOperationStart;
WriteOperationStart:
    // wait until DEN goes low
    loop_until_bit_is_set(EIFR, INTF4);
    if (bit_is_set(EIFR, INTF6)) {
        // change direction to input since we are doing read -> write
        DataInterface::setLowerDataLinesDirection(0xff);
        DataInterface::setUpperDataLinesDirection(0xff);
        // then jump into the write loop
        goto ReadOperationBypass;
    } 

WriteOperationBypass:
    EIFR = 0b0111'0000;
    if constexpr (EnableTransactionDebug) {
        Serial.print(F("W 0x")); 
        Serial.println(AddressLinesInterface.view32.data, HEX);
    }
    doIOOperation<false>();
    goto WriteOperationStart;
}



