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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_EPD.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Adafruit_SI5351.h>
#include <Adafruit_CCS811.h>
#include <RTClib.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <Adafruit_seesaw.h>
#include <Adafruit_MCP9808.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_AS7341.h>
#include <hp_BH1750.h>
#include <SparkFun_Alphanumeric_Display.h>
#include <Adafruit_PCF8591.h>
#include <Adafruit_DS1841.h>
#include <Adafruit_LTR390.h>
#include <Adafruit_APDS9960.h>
#include <Adafruit_EMC2101.h>
#include <Adafruit_Si7021.h>
#include <seesaw_neopixel.h>


#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;

enum class EnabledDisplays {
    SSD1351_OLED_128_x_128_1_5,
    SSD1680_EPaper_250_x_122_2_13,
    ILI9341_TFT_240_x_320_2_8_Capacitive_TS,
};
enum class DisplayKind {
    Unknown,
    TFT,
    OLED,
    EPaper,
};
constexpr auto ActiveDisplay = EnabledDisplays::SSD1351_OLED_128_x_128_1_5;
constexpr auto EPAPER_COLOR_BLACK = EPD_BLACK;
constexpr auto EPAPER_COLOR_RED = EPD_RED;
constexpr bool XINT1DirectConnect = false;
constexpr bool XINT2DirectConnect = false;
constexpr bool XINT3DirectConnect = false;
constexpr bool XINT4DirectConnect = false;
constexpr bool XINT5DirectConnect = false;
constexpr bool XINT6DirectConnect = false;
constexpr bool XINT7DirectConnect = false;
constexpr bool PrintBanner = true;
constexpr bool SupportNewRAMLayout = false;
constexpr bool HybridWideMemorySupported = false;
constexpr auto TransferBufferSize = 16384;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;

constexpr uintptr_t MemoryWindowBaseAddress = SupportNewRAMLayout ? 0x8000 : 0x4000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;

constexpr bool UseOldAuxHardwareBoard = false;
constexpr bool SupportDirectControlSignalConnection = true;

static_assert((MemoryWindowMask == 0x7FFF || MemoryWindowMask == 0x3FFF), "MemoryWindowMask is not right");

constexpr auto displayHasTouchScreen() noexcept {
    switch (ActiveDisplay) {
        case EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS:
            return true;
        default:
            return false;
    }
}

constexpr auto getDisplayTechnology() noexcept {
    switch (ActiveDisplay) {
        case EnabledDisplays::SSD1351_OLED_128_x_128_1_5:
            return DisplayKind::OLED;
        case EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS:
            return DisplayKind::TFT;
        case EnabledDisplays::SSD1680_EPaper_250_x_122_2_13:
            return DisplayKind::EPaper;
        default:
            return DisplayKind::Unknown;
    }
}

Adafruit_SSD1351 oled(
        128,
        128,
        &SPI, 
        EyeSpi::Pins::TFTCS,
        EyeSpi::Pins::DC,
        EyeSpi::Pins::Reset);

Adafruit_SSD1680 epaperDisplay213(
        250, 
        122,
        EyeSpi::Pins::DC, 
        EyeSpi::Pins::Reset,
        EyeSpi::Pins::TFTCS,
        EyeSpi::Pins::MEMCS,
        EyeSpi::Pins::Busy,
        &SPI);
Adafruit_ILI9341 tft_ILI9341(&SPI, 
        EyeSpi::Pins::DC, 
        EyeSpi::Pins::TFTCS, 
        EyeSpi::Pins::RST);
Adafruit_FT6206 ts;

template<typename T>
struct OptionalDevice {
    constexpr bool found() const noexcept { return _found; }
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    template<typename ... Args>
    OptionalDevice(Args&& ... parameters) : _device(parameters...) { }
    void begin() noexcept {
        _found = _device.begin();
    }
    private:
        T _device;
        bool _found = false;
};

template<>
struct OptionalDevice<RTC_PCF8523> {
    using T = RTC_PCF8523;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    constexpr bool found() const noexcept { return _found; }
    constexpr bool initialized() const noexcept { return _initialized; }
    RTC_PCF8523& getDevice() noexcept { return _device; }
    const RTC_PCF8523& getDevice() const noexcept { return _device; }
    void begin() noexcept {
        _found = _device.begin();
        if (_found) {
            if (!_device.initialized() || _device.lostPower()) {
                _device.adjust(DateTime(F(__DATE__), F(__TIME__)));
                _initialized = false;
            } else {
                _initialized = true;
            }
            _device.start();
            // compensate for rtc drifiting (taken from the example program)
            float drift = 43;  // plus or minus over observation period - set to 0
                               // to cancel previous calibration
            float periodSeconds = (7 * 86400); // total observation period in
                                               // sections
            float deviationPPM = (drift / periodSeconds * 1'000'000); // deviation
                                                                      // in parts
                                                                      // per
                                                                      // million
            float driftUnit = 4.34; // use with offset mode PCF8523_TwoHours

            int offset = round(deviationPPM / driftUnit); 
            _device.calibrate(PCF8523_TwoHours, offset); // perform calibration once
                                                         // drift (seconds) and
                                                         // observation period (seconds)
                                                         // are correct
        }
    }
    private:
        RTC_PCF8523 _device;
        bool _found = false;
        bool _initialized = false;
};

template<>
struct OptionalDevice<SFE_MAX1704X> {
    using T = SFE_MAX1704X;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    constexpr operator bool() const noexcept { return _found; }
    void setFound(bool found) noexcept { _found = found; }
    constexpr bool found() const noexcept { return _found; }
    SFE_MAX1704X& getDevice() noexcept { return _device; }
    const SFE_MAX1704X& getDevice() const noexcept { return _device; }
    OptionalDevice(decltype(MAX1704X_MAX17048) kind, int alertThreshold = 20) : _device(kind), _defaultAlertThreshold(alertThreshold) { }
    void begin() noexcept {
        _found = _device.begin();
        if (_found) {
            _device.quickStart();
            _device.setThreshold(_defaultAlertThreshold);
        }
    }
    private:
        SFE_MAX1704X _device;
        bool _found = false;
        int _defaultAlertThreshold;
};


template<typename T>
void
displayPrintln(T value) noexcept {
    if constexpr (ActiveDisplay == EnabledDisplays::SSD1351_OLED_128_x_128_1_5) {
        oled.println(value);
        oled.enableDisplay(true);
    } else if constexpr (ActiveDisplay == EnabledDisplays::SSD1680_EPaper_250_x_122_2_13) {
        epaperDisplay213.println(value);
        epaperDisplay213.display();
    } else if constexpr (ActiveDisplay == EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS) {
        tft_ILI9341.println(value);
    }
}


void
setupDisplay() noexcept {
    if constexpr (ActiveDisplay == EnabledDisplays::SSD1351_OLED_128_x_128_1_5) {
        oled.begin();
        oled.setFont();
        oled.fillScreen(0);
        oled.setTextColor(0xFFFF);
        oled.setTextSize(1);
    } else if constexpr (ActiveDisplay == EnabledDisplays::SSD1680_EPaper_250_x_122_2_13) {
        epaperDisplay213.begin();
        epaperDisplay213.clearBuffer();
        epaperDisplay213.setCursor(0, 0);
        epaperDisplay213.setTextColor(EPAPER_COLOR_BLACK);
        epaperDisplay213.setTextWrap(true);
        epaperDisplay213.setTextSize(2);
    } else if constexpr (ActiveDisplay == EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS) {
        ts.begin();
        tft_ILI9341.begin();
        tft_ILI9341.fillScreen(ILI9341_BLACK);
        tft_ILI9341.setCursor(0, 0);
        tft_ILI9341.setTextColor(ILI9341_WHITE);  
        tft_ILI9341.setTextSize(2);
    }
    displayPrintln(F("i960"));
}
template<>
struct OptionalDevice<Adafruit_SI5351> {
    using T = Adafruit_SI5351;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    explicit constexpr operator bool() const noexcept { return _found; }
    constexpr bool found() const noexcept { return _found; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    void begin() noexcept {
        _found = (_device.begin() == ERROR_NONE);
    }
    private:
        T _device;
        bool _found = false;
};

template<>
struct OptionalDevice<Adafruit_MCP9808> {
    using T = Adafruit_MCP9808;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    constexpr bool found() const noexcept { return _found; }
    constexpr auto getDeviceIndex() const noexcept { return _deviceIndex; }
    constexpr auto getDefaultResolution() const noexcept { return _defaultResolution; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    OptionalDevice(int index = 0x18, int defaultResolution = 3) : _device(), _deviceIndex(index), _defaultResolution(defaultResolution) { }
    void begin() noexcept {
        _found = _device.begin(_deviceIndex);
        if (_found) {
            _device.setResolution(_defaultResolution);
        }
    }
    private:
        T _device;
        int _deviceIndex;
        int _defaultResolution;
        bool _found = false;
};

template<>
struct OptionalDevice<hp_BH1750> {
    using T = hp_BH1750;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    constexpr bool found() const noexcept { return _found; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    void begin() noexcept {
        _found = _device.begin(BH1750_TO_GROUND);
    }
    private:
        T _device;
        bool _found = false;
};

template<>
struct OptionalDevice<Adafruit_PCF8591> {
    using T = Adafruit_PCF8591;
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    constexpr bool found() const noexcept { return _found; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    void begin() noexcept {
        _found = _device.begin();
        if (_found) {
            _device.enableDAC(true);
        }
    }
    private:
        T _device;
        bool _found = false;
};
template<typename T, uint16_t pid>
class SeesawDevice {
    public:
    SeesawDevice(uint8_t address) : _address(address) { }
    [[nodiscard]] constexpr auto getAddress() const noexcept { return _address; }
    [[nodiscard]] auto& underlyingDevice() noexcept { return _device; }
    [[nodiscard]] const auto& underlyingDevice() const noexcept { return _device; }
    [[nodiscard]] bool begin() noexcept {
        return _device.begin(_address) 
            && (static_cast<uint16_t>((_device.getVersion() >> 16) & 0xFFFF) == pid) 
            && static_cast<T*>(this)->begin_impl();
    }
    void pinModeBulk(uint32_t mask, auto kind) noexcept { _device.pinModeBulk(mask, kind); }
    void setGPIOInterrupts(uint32_t mask, int value) noexcept { _device.setGPIOInterrupts(mask, value); }
    uint32_t digitalReadBulk(uint32_t mask) noexcept { return _device.digitalReadBulk(mask); }
    decltype(auto) analogRead(auto index) noexcept { return _device.analogRead(index); }
    private:
        uint8_t _address;
        Adafruit_seesaw _device;
};
class GamepadQT : public SeesawDevice<GamepadQT, 5743> {
    public:
        enum class Buttons {
            X = 6,
            Y = 2,
            A = 5,
            B = 1,
            Select = 0,
            Start = 16,
        };
#define X(value) static constexpr uint32_t Button ## value ## Mask = (1UL << static_cast<uint32_t>(Buttons:: value ))
        X(A);
        X(B);
        X(X);
        X(Y);
        X(Select);
        X(Start);
#undef X
        static constexpr uint32_t ButtonMask = ButtonAMask | ButtonBMask | ButtonXMask | ButtonYMask | ButtonStartMask | ButtonSelectMask;
        struct ButtonResults {
            constexpr ButtonResults(uint32_t raw) noexcept : _raw(raw) { }
            [[nodiscard]] constexpr auto rawData() const noexcept { return _raw; }
#define X(value) [[nodiscard]] constexpr bool buttonPressed_ ## value () const noexcept { return !(_raw & Button ## value ## Mask); } 
            X(A);
            X(B);
            X(X);
            X(Y);
            X(Select);
            X(Start);
#undef X
            private:
            uint32_t _raw;
        };
    public:
        using Parent = SeesawDevice<GamepadQT, 5743>;
        GamepadQT(uint8_t index = 0x50) : Parent(index) { }
        bool begin_impl() noexcept {
            pinModeBulk(ButtonMask, INPUT_PULLUP);
            setGPIOInterrupts(ButtonMask, 1);
            // optionally we can hook this up to an IRQ_PIN if desired
            return true;
        }
        [[nodiscard]] int getJoystickX() noexcept { return analogRead(14); }
        [[nodiscard]] int getJoystickY() noexcept { return analogRead(15); }
        [[nodiscard]] ButtonResults readButtons() noexcept { return ButtonResults{digitalReadBulk(ButtonMask) }; }
};

class NeoSlider : public SeesawDevice<NeoSlider, 5295> {
    public:
        static constexpr auto DefaultI2CAddress = 0x30;
        static constexpr auto AnalogIn = 18;
        static constexpr auto NeoPixelOut = 14;
        static constexpr auto NeoPixelCount = 4;
        using Parent = SeesawDevice<NeoSlider, 5295>;
        NeoSlider(uint8_t index = DefaultI2CAddress) noexcept : Parent(index) { }
        [[nodiscard]] auto readSliderValue() noexcept { return analogRead(AnalogIn); }
        [[nodiscard]] auto& pixelDevice() noexcept { return _pixels; }
        [[nodiscard]] const auto& pixelDevice() const noexcept { return _pixels; }
        
        [[nodiscard]] bool begin_impl() noexcept {
            return _pixels.begin(getAddress());
        }
    private:
        seesaw_NeoPixel _pixels{NeoPixelCount, NeoPixelOut, NEO_GRB + NEO_KHZ800};
};

class PCJoystickPort : public SeesawDevice<PCJoystickPort, 5753> {
    public:
        static constexpr auto Button1 = 3;
        static constexpr auto Button2 = 13;
        static constexpr auto Button3 = 2;
        static constexpr auto Button4 = 14;
        static constexpr uint32_t ButtonMask = (1UL << Button1) |
                                               (1UL << Button2) |
                                               (1UL << Button3) |
                                               (1UL << Button4);
        static constexpr auto JOY1_X = 1;
        static constexpr auto JOY1_Y = 15;
        static constexpr auto JOY2_X = 0;
        static constexpr auto JOY2_Y = 16;
        static constexpr auto DefaultI2CAddress = 0x49;
        struct ButtonResult {
            constexpr ButtonResult(uint32_t value) noexcept : _value(value) { }
            constexpr auto getRawValue() const noexcept { return _value; }
            constexpr auto button1Pressed() const noexcept { return !(_value & (1UL << Button1)); }
            constexpr auto button2Pressed() const noexcept { return !(_value & (1UL << Button2)); }
            constexpr auto button3Pressed() const noexcept { return !(_value & (1UL << Button3)); }
            constexpr auto button4Pressed() const noexcept { return !(_value & (1UL << Button4)); }
            private:
            uint32_t _value;
        };
        using Parent = SeesawDevice<PCJoystickPort, 5753>;
        PCJoystickPort() noexcept : Parent(DefaultI2CAddress) { }
        [[nodiscard]] bool begin_impl() noexcept {
            pinModeBulk(ButtonMask, INPUT_PULLUP);
            setGPIOInterrupts(ButtonMask, 1);
            return true;
        }
        [[nodiscard]] auto readJoy1_X() noexcept { return analogRead(JOY1_X); }
        [[nodiscard]] auto readJoy1_Y() noexcept { return analogRead(JOY1_Y); }
        [[nodiscard]] auto readJoy2_X() noexcept { return analogRead(JOY2_X); }
        [[nodiscard]] auto readJoy2_Y() noexcept { return analogRead(JOY2_Y); }
        [[nodiscard]] ButtonResult readButtons() noexcept { return ButtonResult{digitalReadBulk(ButtonMask) }; }
};



OptionalDevice<RTC_PCF8523> rtc;
OptionalDevice<Adafruit_SI5351> clockgen;
OptionalDevice<Adafruit_CCS811> ccs;
OptionalDevice<SFE_MAX1704X> lipo(MAX1704X_MAX17048); // Qwiic Fuel Gauge (SPARKX)
OptionalDevice<Adafruit_seesaw> seesaw;
OptionalDevice<Adafruit_MCP9808> tempSensor0(0x18, 3);
OptionalDevice<Adafruit_AHTX0> aht;
OptionalDevice<Adafruit_AS7341> as7341;
OptionalDevice<hp_BH1750> bh1750;
OptionalDevice<HT16K33> alpha7Display;
OptionalDevice<Adafruit_PCF8591> pcfADC_DAC;
OptionalDevice<Adafruit_DS1841> ds;
OptionalDevice<Adafruit_LTR390> ltr;
OptionalDevice<Adafruit_APDS9960> apds;
OptionalDevice<Adafruit_EMC2101> fan0;
OptionalDevice<Adafruit_Si7021> si7021;
OptionalDevice<NeoSlider> sliders[16] { 
#define X(offset) { 0x30 + offset }
    X(0), X(1), X(2), X(3),
    X(4), X(5), X(6), X(7),
    X(8), X(9), X(10), X(11),
    X(12), X(13), X(14), X(15),
#undef X
};
OptionalDevice<GamepadQT> gamepads[4] {
#define X(offset) { 0x50 + offset } 
X(0),
X(1),
X(2),
X(3),
#undef X
};
OptionalDevice<PCJoystickPort> joystick;
void 
setupDevices() noexcept {
    setupDisplay();
    rtc.begin();
    aht.begin();
    as7341.begin();
    lipo.begin();
    seesaw.begin();
    ccs.begin();
    clockgen.begin();
    tempSensor0.begin();
    bh1750.begin();
    alpha7Display.begin();
    pcfADC_DAC.begin();
    ds.begin();
    ltr.begin();
    apds.begin();
    fan0.begin();
    si7021.begin();
    for (auto& a : sliders) {
        a.begin();
    }
    for (auto& a : gamepads) {
        a.begin();
    }
    joystick.begin();
}
[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2210)]] inline volatile CH351 ControlSignals;
[[gnu::address(0x2218)]] inline volatile CH351 XBUSBankRegister;
[[gnu::address(0x2208)]] volatile uint8_t dataLines[4];
[[gnu::address(0x2208)]] volatile uint32_t dataLinesFull;
[[gnu::address(0x2208)]] volatile uint16_t dataLinesHalves[2];
[[gnu::address(0x220C)]] volatile uint32_t dataLinesDirection;
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_bytes[4];
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_LSB;

[[gnu::address(0x2200)]] volatile uint16_t AddressLines16Ptr[4];
[[gnu::address(0x2200)]] volatile uint32_t AddressLines32Ptr[2];
[[gnu::address(0x2200)]] volatile uint32_t addressLinesValue32;
[[gnu::address(0x2200)]] volatile uint16_t addressLinesLowerHalf;
[[gnu::address(0x2200)]] volatile uint8_t addressLines[8];
[[gnu::address(0x2200)]] volatile uint8_t addressLinesLowest;
[[gnu::address(0x2200)]] volatile uint24_t addressLinesLower24;
// allocate 1024 bytes total
[[gnu::always_inline]] inline bool isBurstLast() noexcept { 
    return digitalRead<Pin::BLAST>() == LOW; 
}
[[gnu::always_inline]]
inline
void 
setBankIndex(uint8_t value) {
    getOutputRegister<Port::IBUS_Bank>() = value;
}

template<NativeBusWidth width>
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}
constexpr
uint16_t
computeTransactionWindow(uint16_t offset) noexcept {
    return MemoryWindowBaseAddress | (offset & MemoryWindowMask);
}

[[gnu::used]]
FORCE_INLINE
inline
DataRegister8
getTransactionWindow() noexcept {
    if constexpr (SupportNewRAMLayout) {
        setBankIndex(getInputRegister<Port::BankCapture>());
        return memoryPointer<uint8_t>(computeTransactionWindow(addressLinesLowerHalf));
    } else {
        SplitWord32 split{addressLinesLower24};
        setBankIndex(split.getIBUSBankIndex());
        return memoryPointer<uint8_t>(computeTransactionWindow(split.halves[0]));
    }
}


template<bool waitForReady = false>
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    //pulse<Pin::READY>();
    toggle<Pin::READY>();
    if constexpr (waitForReady) {
        // wait four cycles after to make sure that the ready signal has been
        // propagated to the i960
        insertCustomNopCount<4>();
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
    if constexpr (UseOldAuxHardwareBoard) {
        ControlSignals.ctl.reset = LOW;
    } else {
        digitalWrite<Pin::RESET, LOW>();
    }
}
void 
pullCPUOutOfReset() noexcept {
    if constexpr (UseOldAuxHardwareBoard) {
        ControlSignals.ctl.reset = HIGH;
    } else {
        digitalWrite<Pin::RESET, HIGH>();
    }
}



template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 4, "Invalid index provided to setDataByte, must be less than 4");
    if constexpr (index < 4) {
        dataLines[index] = value;
    }
}

FORCE_INLINE
inline void setDataByte(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
    setDataByte<0>(a);
    setDataByte<1>(b);
    setDataByte<2>(c);
    setDataByte<3>(d);
}

template<uint8_t index>
inline uint8_t getDataByte() noexcept {
    static_assert(index < 4, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 4) {
        return dataLines[index];
    } else {
        return 0;
    }
}
template<uint8_t value>
[[gnu::always_inline]]
inline 
void 
updateDataLinesDirection() noexcept {
    dataLinesDirection_bytes[0] = value;
    dataLinesDirection_bytes[1] = value;
    dataLinesDirection_bytes[2] = value;
    dataLinesDirection_bytes[3] = value;
}

/**
 * @brief Just go through the motions of a write operation but do not capture
 * anything being sent by the i960
 */
FORCE_INLINE
inline void 
idleTransaction() noexcept {
    // just keep going until we are done
    while (true) {
        if (isBurstLast()) {
            signalReady<true>();
            return;
        }
        signalReady<true>();
    }
}
template<bool isReadOperation, NativeBusWidth width>
struct CommunicationKernel {
    using Self = CommunicationKernel<isReadOperation, width>;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

FORCE_INLINE
inline
static void
doCommunication() noexcept {
        auto theBytes = getTransactionWindow(); 
        if constexpr (isReadOperation) {
            DataRegister32 view32 = reinterpret_cast<DataRegister32>(theBytes);
            dataLinesFull = view32[0];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[1];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[2];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[3];
        } else {
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[0] = getDataByte<0>();
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[1] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[2] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[3] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte since we
            // flowed into this
            theBytes[4] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[5] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[6] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[7] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte since we
            // flowed into this
            theBytes[8] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[9] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[10] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[11] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte!
            theBytes[12] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[13] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[14] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[15] = getDataByte<3>();
            } 
        }
Done:
        signalReady<true>();

}
FORCE_INLINE 
inline 
static void doIO() noexcept { 
        switch (addressLines[0]) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            dataLinesFull = F_CPU;
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            dataLinesFull = F_CPU / 2;
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = Serial.read();
                            dataLinesHalves[1] = 0;
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(dataLinesHalves[0]));
                        } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 12: { 
                         Serial.flush();
                         if constexpr (isReadOperation) { 
                             dataLinesFull = 0;
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
                     break;
#define X(obj, index) \
            case index + 0: { \
                        /* TCCRnA and TCCRnB */ \
                        if constexpr (isReadOperation) { \
                            setDataByte(obj.TCCRxA, obj.TCCRxB, obj.TCCRxC, 0);\
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                obj.TCCRxA = getDataByte<0>();\
                            } \
                            if (digitalRead<Pin::BE1>() == LOW) { \
                                obj.TCCRxB = getDataByte<1>();\
                            } \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                obj.TCCRxC = getDataByte<2>();\
                            } \
                        } \
                        if (isBurstLast()) {\
                            break; \
                        }\
                        signalReady<true>();  \
                    } \
            case index + 4: { \
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.TCNTx;\
                            auto tmp2 = obj.ICRx;\
                            interrupts(); \
                            dataLinesHalves[0] = tmp; \
                            dataLinesHalves[1] = tmp2;\
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW && \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
                                noInterrupts(); \
                                obj.TCNTx = value;\
                                interrupts(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.ICRx = value;\
                                interrupts(); \
                            } \
                        } \
                        if (isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case index + 8: { \
                        /* OCRnA should only be accessible if you do a full 16-bit write */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.OCRxA;\
                             auto tmp2 = obj.OCRxB;\
                            interrupts(); \
                            dataLinesHalves[0] = tmp; \
                            dataLinesHalves[1] = tmp2; \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
                                noInterrupts(); \
                                obj.OCRxA = value;\
                                interrupts(); \
                            } \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.OCRxB = value; \
                                interrupts(); \
                             } \
                        } \
                        if (isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case index + 12: { \
                         /* OCRnC */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             auto tmp = obj.OCRxC; \
                             interrupts(); \
                             dataLinesHalves[0] = tmp; \
                             dataLinesHalves[1] = 0;\
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = dataLinesHalves[0]; \
                                  noInterrupts(); \
                                  obj.OCRxC = value;\
                                  interrupts(); \
                              }\
                         } \
                         if (isBurstLast()) {\
                             break;\
                         } \
                         signalReady<true>(); \
                         break;\
                     } 
#ifdef TCCR1A
                    X(timer1, 0x10);
#endif
#ifdef TCCR3A
                    X(timer3, 0x20);
#endif
#ifdef TCCR4A
                    X(timer4, 0x30);
#endif
#ifdef TCCR5A
                    X(timer5, 0x40);
#endif
#undef X
            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
}
};

template<bool isReadOperation>
struct CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen> {
    using Self = CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen>;
    static constexpr auto BusWidth = NativeBusWidth::Sixteen;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

public:
    //[[gnu::optimize("no-reorder-blocks")]]
    FORCE_INLINE
    inline
    static void
    doCommunication() noexcept {
        auto theBytes = getTransactionWindow(); 
        // figure out which word we are currently looking at
        // if we are aligned to 32-bit word boundaries then just assume we
        // are at the start of the 16-byte block (the processor will stop
        // when it doesn't need data anymore). If we are not then skip over
        // this first two bytes and start at the upper two bytes of the
        // current word.
        //
        // I am also exploiting the fact that the processor can only ever
        // accept up to 16-bytes at a time if it is aligned to 16-byte
        // boundaries. If it is unaligned then the operation is broken up
        // into multiple transactions within the i960 itself. So yes, this
        // code will go out of bounds but it doesn't matter because the
        // processor will never go out of bounds.


        // The later field is used to denote if the given part of the
        // transaction is later on in burst. If it is then we will
        // terminate early without evaluating BLAST if the upper byte
        // enable is high. This is because if we hit 0b1001 this would be
        // broken up into two 16-bit values (0b1101 and 0b1011) which is
        // fine but in all cases the first 1 we encounter after finding the
        // first zero in the byte enable bits we are going to terminate
        // anyway. So don't waste time evaluating BLAST at all!
        //
        // since we are using the pointer directly we have to be a little more
        // creative. The base offsets have been modified
        
        if constexpr (isReadOperation) {
            auto a = theBytes[0];
            auto b = theBytes[1];
            if (isBurstLast()) {
                uint8_t baseIndex = 0;
                if (digitalRead<Pin::AlignmentCheck>() != LOW) {
                    baseIndex = 2;
                }
                dataLines[baseIndex] = a;
                dataLines[baseIndex+1] = b;
                goto Done;
            } else {
                auto c = theBytes[2];
                auto d = theBytes[3];
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    dataLines[0] = a;
                    dataLines[1] = b;
                    signalReady<true>();
                    dataLines[2] = c;
                    dataLines[3] = d;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto e = theBytes[4];
                    auto f = theBytes[5];
                    signalReady<true>();
                    dataLines[0] = e;
                    dataLines[1] = f;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto g = theBytes[6];
                    auto h = theBytes[7];
                    signalReady<true>();
                    dataLines[2] = g;
                    dataLines[3] = h;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto i = theBytes[8];
                    auto j = theBytes[9];
                    signalReady<true>();
                    dataLines[0] = i;
                    dataLines[1] = j;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto k = theBytes[10];
                    auto l = theBytes[11];
                    signalReady<true>();
                    dataLines[2] = k;
                    dataLines[3] = l;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto m = theBytes[12];
                    auto n = theBytes[13];
                    signalReady<true>();
                    dataLines[0] = m;
                    dataLines[1] = n;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto o = theBytes[14];
                    auto p = theBytes[15];
                    signalReady<true>();
                    dataLines[2] = o;
                    dataLines[3] = p;
                    goto Done;
                } else {
                    setDataByte(c, d, a, b);
                    signalReady<true>();
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<false>();
                    setDataByte(theBytes[6], theBytes[7], theBytes[4], theBytes[5]);
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<true>();
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<false>();
                    setDataByte(theBytes[10], theBytes[11], theBytes[8], theBytes[9]);
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<true>();
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<false>();
                    setDataByte(theBytes[14], theBytes[15], theBytes[12], theBytes[13]);
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    signalReady<true>();
                    goto Done;
                }
            }
        } else {
            if (isBurstLast()) {
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    auto w = dataLines[0];
                    auto x = dataLines[1];
                    if (digitalRead<Pin::BE0>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    if (digitalRead<Pin::BE1>() == LOW) {
                        theBytes[1] = x;
                    }
                } else {
                    auto w = dataLines[2];
                    auto x = dataLines[3];
                    if (digitalRead<Pin::BE2>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[1] = x;
                    }
                }
                goto Done;
            } else {
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    // defer writes as much as possible
                    auto w = dataLines[0];
                    auto x = dataLines[1];
                    if (digitalRead<Pin::BE0>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    theBytes[1] = x;
                    // we only need to check to see if BE0 is enabled
                    // BE1 must be enabled since we are going to flow into
                    // the next byte
                    signalReady<true>();
                    auto y = dataLines[2];
                    auto z = dataLines[3];
                    theBytes[2] = y;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[3] = z;
                    }
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        goto Done;
                    } 
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    theBytes[4] = dataLines[0];
                    auto b = dataLines[1];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[5] = b ;
                    }
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[6] = dataLines[2];
                    auto d = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[7] = d;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the enable lines
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto f = dataLines[1];
                    theBytes[8] = dataLines[0];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[9] = f;
                    }
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[10] = dataLines[2];
                    auto h = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[11] = h;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    theBytes[12] = dataLines[0];
                    auto j = dataLines[1];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[13] = j;
                    }
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[14] = dataLines[2];
                    auto l = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[15] = l;
                    }
                    goto Done;
                } else {
                    auto a = dataLines[2];
                    auto b = dataLines[3];
                    if (digitalRead<Pin::BE2>() == LOW) [[gnu::likely]] {
                        theBytes[0] = a;
                    }
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[1] = b;
                    }
                    signalReady<true>();
                    auto c = dataLines[0];
                    auto d = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[2] = c;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[3] = d;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto e = dataLines[2];
                    auto f = dataLines[3];
                    theBytes[4] = e;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[5] = f;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto g = dataLines[0];
                    auto h = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[6] = g;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[7] = h;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto i = dataLines[2];
                    auto j = dataLines[3];
                    theBytes[8] = i;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[9] = j;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto k = dataLines[0];
                    auto l = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[10] = k;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[11] = l;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto m = dataLines[2];
                    auto n = dataLines[3];
                    theBytes[12] = m;
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[13] = n;
                    }
                    goto Done;
                }
            }
        }
Done:
        signalReady<true>();
    }
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady<true>()

FORCE_INLINE 
inline 
static void doIO() noexcept { 
        switch (addressLines[0]) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU);
                        } 
                        I960_Signal_Switch;
                    } 
            case 2: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU) >> 16);
                        } 
                        I960_Signal_Switch;
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU / 2);
                        } 
                        I960_Signal_Switch;
                    } 
            case 6: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU / 2) >> 16);
                        } 
                        I960_Signal_Switch;
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = Serial.read();
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(getDataByte<0>()));
                        } 
                        I960_Signal_Switch;
                    } 
            case 10: {
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[1] = 0;
                         } 
                        I960_Signal_Switch;
                     } 
            case 12: { 
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[0] = 0; 
                         } else { 
                             Serial.flush();
                         }
                         I960_Signal_Switch;
                     }
            case 14: {
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
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
                            setDataByte<2>(obj.TCCRxC);\
                            setDataByte<3>(0); \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                obj.TCCRxC = getDataByte<2>();\
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
                            dataLinesHalves[0] = tmp;  \
                        } else {  \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) {  \
                                auto value = dataLinesHalves[0];  \
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
                            dataLinesHalves[1] = tmp; \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
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
                            dataLinesHalves[0] = tmp; \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
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
                             dataLinesHalves[1] = tmp; \
                         } else { \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
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
                             dataLinesHalves[0] = tmp; \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = dataLinesHalves[0]; \
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
                            dataLinesHalves[1] = 0; \
                         } \
                         break;\
                     }  
#ifdef TCCR1A
                              X(timer1, 0x10);
#endif
#ifdef TCCR3A
                              X(timer3, 0x20);
#endif
#ifdef TCCR4A
                              X(timer4, 0x30);
#endif
#ifdef TCCR5A
                              X(timer5, 0x40);
#endif
#undef X


            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
}
#undef I960_Signal_Switch
};



template<NativeBusWidth width> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    digitalWrite<Pin::DirectionOutput, HIGH>();
    setBankIndex(0);
    // switch the XBUS bank mode to i960 instead of AVR
    // I want to use the upper four bits the XBUS address lines
    // while I can directly connect to the address lines, I want to test to
    // make sure that this works as well
    if constexpr (UseOldAuxHardwareBoard) {
        ControlSignals.ctl.bankSelect = 1;

        XBUSBankRegister.view32.data = 0;
    }
    // at this point, we are setup to be in output mode (or read) and that is the
    // expected state for _all_ i960 processors, it will load some amount of
    // data from main memory to start the execution process. 
    //
    // After this point, we will never need to actually keep track of the
    // contents of the DirectionOutput pin. We will always be properly
    // synchronized overall!
    //
    // It is not lost on me that this is goto nightmare bingo, however in this
    // case I need the extra control of the goto statement. Allowing the
    // compiler to try and do this instead leads to implicit jumping issues
    // where the compiler has way too much fun with its hands. It will over
    // optimize things and create problems!
ReadOperationStart:
    // wait until DEN goes low
    while (digitalRead<Pin::DEN>());
    // check to see if we need to change directions
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change direction to input since we are doing read -> write
        updateDataLinesDirection<0>();
        // update the direction pin 
        toggle<Pin::DirectionOutput>();
        // then jump into the write loop
        goto WriteOperationBypass;
    }
ReadOperationBypass:
    if constexpr (false) {
        Serial.println(F("Read Operation"));
        Serial.println(addressLinesValue32, HEX);
        Serial.println(addressLinesValue32, HEX);
    }
    // standard read operation so do the normal dispatch
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        if constexpr (HybridWideMemorySupported) {
            idleTransaction();
        } else {
            CommunicationKernel<true, width>::doCommunication();
        }
    } else {
        if (digitalRead<Pin::A23_960>()) {
            CommunicationKernel<true, width>::doCommunication();
        } else {
            // io operation
            CommunicationKernel<true, width>::doIO();
        }
    }
    // start the read operation again
    goto ReadOperationStart;

WriteOperationStart:
    // wait until DEN goes low
    while (digitalRead<Pin::DEN>());
    // check to see if we need to change directions
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change data lines to be output since we are doing write -> read
        updateDataLinesDirection<0xFF>();
        // update the direction pin
        toggle<Pin::DirectionOutput>();
        // jump to the read loop
        goto ReadOperationBypass;
    } 
WriteOperationBypass:
    if constexpr (false) {
        Serial.println(F("Write Operation"));
        Serial.println(addressLinesValue32, HEX);
    }
    // standard write operation so do the normal dispatch for write operations
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        if constexpr (HybridWideMemorySupported) {
            idleTransaction();
        } else {
            CommunicationKernel<false, width>::doCommunication();
        }
    } else {
        if (digitalRead<Pin::A23_960>()) {
            CommunicationKernel<false, width>::doCommunication();
        } else {
            // io operation
            CommunicationKernel<false, width>::doIO();
        }
    }
    // restart the write loop
    goto WriteOperationStart;
    // we should never get here!
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
    // look for firmware.bin and open it readonly
    if (auto theFirmware = SD.open("firmware.bin", FILE_READ); !theFirmware) {
        Serial.println(F("Could not open firmware.bin for reading!"));
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
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            // just modify the bank as we go along
            setBankIndex(view.getIBUSBankIndex());
            auto* theBuffer = memoryPointer<uint8_t>(view.unalignedBankAddress(AccessFromIBUS{}));
            theFirmware.read(const_cast<uint8_t*>(theBuffer), BufferSize);
            Serial.print(F("."));
        }
        Serial.println(F("DONE!"));
        theFirmware.close();
    }
    // okay so now end reading from the SD Card
    SD.end();
    SPI.endTransaction();
}
void 
setupPins() noexcept {
    // power down the ADC and USART3
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate ADC
    PRR1 = 0b00000'100; // deactivate USART3

    // enable interrupt pin output
    pinMode<Pin::INT0_960_>(OUTPUT);
    digitalWrite<Pin::INT0_960_, HIGH>();
    // setup the IBUS bank
    getDirectionRegister<Port::IBUS_Bank>() = 0xFF;
    getOutputRegister<Port::IBUS_Bank>() = 0;
    pinMode(Pin::IsMemorySpaceOperation, INPUT);
    pinMode(Pin::BE0, INPUT);
    pinMode(Pin::BE1, INPUT);
    pinMode(Pin::BE2, INPUT);
    pinMode(Pin::BE3, INPUT);
    pinMode(Pin::DEN, INPUT);
    pinMode(Pin::BLAST, INPUT);
    pinMode(Pin::WR, INPUT);
    pinMode(Pin::DirectionOutput, OUTPUT);
    pinMode(Pin::AlignmentCheck, INPUT);
    pinMode(Pin::A23_960, INPUT);
    // we start with 0xFF for the direction output so reflect it here
    digitalWrite<Pin::DirectionOutput, HIGH>();
    pinMode(Pin::ChangeDirection, INPUT);
    pinMode(Pin::READY, OUTPUT);
    digitalWrite<Pin::READY, HIGH>();
    // setup bank capture to read in address lines
    getDirectionRegister<Port::BankCapture>() = 0;
    if constexpr (SupportDirectControlSignalConnection) {
        pinMode(Pin::HOLD, OUTPUT);
        digitalWrite<Pin::HOLD, LOW>();
        pinMode(Pin::HLDA, INPUT);
        pinMode(Pin::LOCK, INPUT);
        pinMode(Pin::FAIL, INPUT);
        pinMode(Pin::RESET, OUTPUT);
        digitalWrite<Pin::RESET, HIGH>();
        pinMode(Pin::CFG0, INPUT);
        pinMode(Pin::CFG1, INPUT);
        pinMode(Pin::CFG2, INPUT);

        pinMode(Pin::BusQueryEnable, OUTPUT);
        digitalWrite<Pin::BusQueryEnable, HIGH>();
    }
}
void
setupExternalBus() noexcept {
    // setup the EBI
    XMCRB=0b1'0000'000;
    // use the upper and lower sector limits feature to accelerate IO space
    // 0x2200-0x3FFF is full speed, rest has a one cycle during read/write
    // strobe delay since SRAM is 55ns
    //
    // I am using an HC573 on the interface board so the single cycle delay
    // state is necessary! When I replace the interface chip with the
    // AHC573, I'll get rid of the single cycle delay from the lower sector
    XMCRA=0b1'010'01'01;  
    AddressLinesInterface.view32.direction = 0;
    DataLinesInterface.view32.direction = 0xFFFF'FFFF;
    DataLinesInterface.view32.data = 0;
    if constexpr (UseOldAuxHardwareBoard) {
        static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;
        ControlSignals.view32.direction = ControlSignalDirection;
        ControlSignals.view8.direction[1] &= 0b11101111;
        ControlSignals.view8.direction[2] &= 0b11111110;
        if constexpr (XINT1DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b11111101;
        }
        if constexpr (XINT2DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b11111011;
        }
        if constexpr (XINT3DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b11110111;
        }
        if constexpr (XINT4DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b11101111;
        }
        if constexpr (XINT5DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b11011111;
        }
        if constexpr (XINT6DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b10111111;
        }
        if constexpr (XINT7DirectConnect) {
            ControlSignals.view8.direction[2] &= 0b01111111;
        }
        ControlSignals.view32.data = 0;
        ControlSignals.ctl.hold = 0;
        ControlSignals.ctl.reset = 0; // put i960 in reset
        ControlSignals.ctl.backOff = 1;
        ControlSignals.ctl.ready = 0;
        ControlSignals.ctl.nmi = 1;
        ControlSignals.ctl.xint0 = 1;
        ControlSignals.ctl.xint1 = 1;
        ControlSignals.ctl.xint2 = 1;
        ControlSignals.ctl.xint3 = 1;
        ControlSignals.ctl.xint4 = 1;
        ControlSignals.ctl.xint5 = 1;
        ControlSignals.ctl.xint6 = 1;
        ControlSignals.ctl.xint7 = 1;
        // select the CH351 bank chip to go over the xbus address lines
        ControlSignals.ctl.bankSelect = 0;
    }
}
void 
setupPlatform() noexcept {
    setupExternalBus();
    setupDevices();
}

CPUKind 
getInstalledCPUKind() noexcept { 
    if constexpr (UseOldAuxHardwareBoard) {
        return static_cast<CPUKind>(ControlSignals.ctl.cfg); 
    } else {
        return static_cast<CPUKind>((getInputRegister<Port::CTL960>() >> 5) & 0b111);
    }
}
void
printlnBool(bool value) noexcept {
    if (value) {
        Serial.println(F("TRUE"));
    } else {
        Serial.println(F("FALSE"));
    }
}
void banner() noexcept;

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
    Wire.begin();
    setupPins();
    // setup the IO Expanders
    setupPlatform();
    putCPUInReset();
    if constexpr (PrintBanner) {
        banner();
    }
    // find firmware.bin and install it into the 512k block of memory
    installMemoryImage();
    pullCPUOutOfReset();
}
void 
loop() {
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            executionBody<NativeBusWidth::Sixteen>();
            break;
        case NativeBusWidth::ThirtyTwo:
            executionBody<NativeBusWidth::ThirtyTwo>();
            break;
        default:
            executionBody<NativeBusWidth::Unknown>();
            break;
    }
}

template<typename T>
void printlnBool(const T& value) noexcept {
    printlnBool(value.found());
}

void
banner() noexcept {
    Serial.println(F("i960 Chipset"));
    Serial.println(F("(C) 2019-2023 Joshua Scoggins"));
    Serial.println(F("Variant: Type 103 Narrow Wide"));
    constexpr uint32_t IORamMax = MemoryWindowBaseAddress * 256ul; // 256 banks *
                                                                 // window size
    Serial.println(F("Features: "));
    Serial.println(F("Bank Switching Controlled By AVR"));
    Serial.print(F("Base Address of IO RAM Window: 0x"));
    Serial.println(MemoryWindowBaseAddress, HEX);
    Serial.print(F("Maximum IO RAM Available: "));
    Serial.print(IORamMax, DEC);
    Serial.println(F(" bytes"));
    Serial.print(F("Memory Mapping Mode: "));
    if constexpr (HybridWideMemorySupported) {
        Serial.println(F("Directly Connected FLASH/SRAM/RAM + IO Space with Independent RAM Section"));
    } else {
        Serial.println(F("IO RAM Section mapped throughout i960 Memory Space"));
    }
    Serial.print(F("Detected i960 CPU Kind: "));
    switch (getInstalledCPUKind()) {
        case CPUKind::Sx:
            Serial.println(F("Sx"));
            break;
        case CPUKind::Kx:
            Serial.println(F("Kx"));
            break;
        case CPUKind::Jx:
            Serial.println(F("Jx"));
            break;
        case CPUKind::Hx:
            Serial.println(F("Hx"));
            break;
        case CPUKind::Cx:
            Serial.println(F("Cx"));
            break;
        default:
            Serial.println(F("Unknown"));
            break;
    }
    Serial.print(F("Bus Width: "));
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            Serial.println(F("16-bit"));
            break;
        case NativeBusWidth::ThirtyTwo:
            Serial.println(F("32-bit"));
            break;
        default:
            Serial.println(F("Unknown (fallback to 32-bit)"));
            break;
    }
    Serial.println(F("Optional Devices List (i2c)"));
    if (rtc.found()) {
        Serial.println(F("Found RTC (PCF8523)"));
        if (rtc.initialized()) {
            Serial.println(F("\tRTC was already initialized"));
        } else {
            Serial.println(F("\tRTC needed to be initialized!"));
        }
    }
    if (clockgen) {
        Serial.println(F("Found Si5351 Clock Generator"));
        // taken from code example for Si5351
        Serial.println(F("\tSetting PLLA to 900MHz"));
        clockgen->setupPLLInt(SI5351_PLL_A, 36);
        Serial.println(F("\tSet Output #0 to 112.5MHz"));
        clockgen->setupMultisynthInt(0, SI5351_PLL_A, SI5351_MULTISYNTH_DIV_8);


        // fractional mode display
        // setup PLLB to 616.66667 MHz (XTAL * 24 + 2/3)
        // setup multisynth 1 to 13.55311MHz (PLLB/45.5)
        clockgen->setupPLL(SI5351_PLL_B, 24, 2, 3);
        Serial.println(F("\tSet Output #1 to 13.553115MHz"));
        clockgen->setupMultisynth(1, SI5351_PLL_B, 45, 1, 2);

        Serial.println(F("\tSet Output #2 to 10.706 KHz"));
        clockgen->setupMultisynth(2, SI5351_PLL_B, 900, 0, 1);
        clockgen->setupRdiv(2, SI5351_R_DIV_64);

        clockgen->enableOutputs(true);

        delay(1000);
        clockgen->enableOutputs(false);
    }
    if (ccs) {
        Serial.println(F("Found CCS811"));
        if (ccs->available()) {
            if (!ccs->readData()) {
                 Serial.print(F("\tCO2: "));
                 Serial.print(ccs->geteCO2());
                 Serial.println(F("ppm"));
                 Serial.print(F("\tTVOC: "));
                 Serial.println(ccs->getTVOC());
            } else {
                Serial.println(F("\tERROR While attempting to read from CCS811"));
            }
        } else {
            Serial.println(F("\tCCS811 isn't available yet!"));
        }
    }
    if (lipo) {
        Serial.println(F("Found MAX17048"));
        Serial.print(F("\tAlert Threshold: "));
        Serial.println(lipo->getThreshold(), DEC);
    }
    if (seesaw) {
        Serial.println(F("Found seesaw"));
    }

    if (tempSensor0) {
        Serial.println(F("Found MCP9808"));
        Serial.print(F("\tSensor Resolution: "));
        Serial.println(tempSensor0->getResolution());
        tempSensor0->wake();
        Serial.print(F("\tcurrent temperature: "));
        float c = tempSensor0->readTempC();
        Serial.print(c, 4); Serial.println(F("*C"));
        tempSensor0->shutdown_wake(1);
    }

    if (aht) {
        Serial.println(F("Found AHTx0"));
        sensors_event_t humidity, temperature;
        aht->getEvent(&humidity, &temperature);
        Serial.print(F("\tTemperature: "));
        Serial.print(temperature.temperature);
        Serial.println(F(" degrees C"));
        Serial.print(F("\tHumidity: "));
        Serial.print(humidity.relative_humidity);
        Serial.println(F("% rH"));
    }
    if (as7341) {
        Serial.println(F("Found AS7341"));
        Serial.println(F("\t4 mA LED blink"));
        as7341->setLEDCurrent(4); // 4 mA
        as7341->enableLED(true);
        delay(100);
        as7341->enableLED(false);
        delay(500);

        Serial.println(F("\t100 mA LED blink"));
        as7341->setLEDCurrent(100); // 100mA
        as7341->enableLED(true);
        delay(100);
        as7341->enableLED(false);
        delay(500);
    }
    if (bh1750) {
        Serial.println(F("Found BH1750"));
        Serial.print(F("\tCurrent lux: "));
        bh1750->start();
        Serial.println(bh1750->getLux());
    }
    if (alpha7Display) {
        Serial.println(F("Found HT16K33"));
        alpha7Display->printChar('i', 0);
        alpha7Display->printChar('9', 1);
        alpha7Display->printChar('6', 2);
        alpha7Display->printChar('0', 3);
        alpha7Display->updateDisplay();
    }
    if (pcfADC_DAC) {
        Serial.println(F("Found PCF8591"));
        /// @todo implement demo version?
    }
    if (ds) {
        Serial.println(F("Found DS1841"));
        Serial.println(F("\tSet Wiper to 10"));
        ds->setWiper(10);
        delay(1000);
        Serial.print(F("\tTemperature: ")); Serial.print(ds->getTemperature()); Serial.println(F(" degrees C"));
        Serial.print(F("\tWiper: ")); Serial.print(ds->getWiper()); Serial.println(F(" LSB"));
        Serial.println();
        Serial.println(F("\tSet Wiper to 120"));
        ds->setWiper(120);
        delay(1000);
        Serial.print(F("\tTemperature: ")); Serial.print(ds->getTemperature()); Serial.println(F(" degrees C"));
        Serial.print(F("\tWiper: ")); Serial.print(ds->getWiper()); Serial.println(F(" LSB"));
        delay(1000);
        ds->setWiper(0);
    }
    if (ltr) {
        Serial.println(F("Found LTR390"));
        /// @todo LTR390 display / test
    }
    if (apds) {
        Serial.println(F("Found APDS9960"));
        /// @todo APDS9960 display / test
    }
    if (fan0) {
        Serial.println(F("Found EMC2101"));
        /// @todo elaborate
    }
    if (si7021) {
        Serial.println(F("Found SI70xx Series Chip"));
        Serial.print(F("\tModel: "));
        switch (si7021->getModel()) {
            case SI_Engineering_Samples:
                Serial.println(F("SI engineering samples"));
                break;
            case SI_7013:
                Serial.println(F("Si7013"));
                break;
            case SI_7020:
                Serial.println(F("Si7020"));
                break;
            case SI_7021:
                Serial.println(F("Si7021"));
                break;
            case SI_UNKNOWN:
            default:
                Serial.println(F("UNKNOWN"));
                break;
        }
        Serial.print(F("\tRevision: "));
        Serial.println(si7021->getRevision());
        Serial.print(F("\tSerial #: "));
        Serial.print(si7021->sernum_a, HEX);
        Serial.println(si7021->sernum_b, HEX);
        Serial.print(F("\tHumidity: "));
        Serial.println(si7021->readHumidity(), 2);
        Serial.print(F("\tTemperature: "));
        Serial.print(si7021->readTemperature(), 2);
        Serial.println(F(" degrees C"));
    }
}
