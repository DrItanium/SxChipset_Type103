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
#include <Event.h>
#include <Logic.h>
#include <Wire.h>
#include <EEPROM.h>
#include <SPI.h>
// The goal of this program is to be a deeply embeded ATMEGA4809 responsible
// for generating the 20MHz, 10MHz, and 5MHz clocks used by the i960 system.
// Once powered on, it will continue to run until power is removed. This makes
// it an ideal device for anything that needs to be available 100% of the time

// PORTA contains all useful stuff so keep it reserved for that
constexpr auto DEBUG_TXD = PIN_PA0;
constexpr auto DEBUG_RXD = PIN_PA1;
constexpr auto TWI_SDA = PIN_PA2;
constexpr auto TWI_SCL = PIN_PA3;
constexpr auto SPI_MOSI = PIN_PA4;
constexpr auto SPI_MISO = PIN_PA5;
constexpr auto SPI_SCK = PIN_PA6;
constexpr auto CLKOUT = PIN_PA7;
constexpr auto NumberOfAnalogChannels = 11;
constexpr decltype(PIN_PD0) AnalogChannels[NumberOfAnalogChannels] {
    PIN_PD0,
    PIN_PD1,
    PIN_PD2,
    PIN_PD3,
    PIN_PD4,
    PIN_PD5,
    PIN_PD6,
    PIN_PD7,
    PIN_PF3,
    PIN_PF4,
    PIN_PF5,
};
constexpr auto ClockConfigurationBit = PIN_PF6;
constexpr auto getClock2_960() noexcept {
#ifdef REDUCED_HARDWARE
    return PIN_PC2;
#else
    return PIN_PE2;    
#endif
}
constexpr auto CLK960_2 = getClock2_960();
constexpr auto CLK960_1 = PIN_PF2;
constexpr auto MicrocontrollerClockRate = F_CPU;
volatile uint32_t I960CLK2Rate = F_CPU;
volatile uint32_t I960CLKRate = F_CPU / 2;
void
configureCLK2(Event& evt) {
    evt.set_user(
#ifdef REDUCED_HARDWARE
    user::evoutc_pin_pc2
#else
    user::evoute_pin_pe2
#endif
    );
}
void
configureCLK(Event& evt) {
    evt.set_user(user::evoutf_pin_pf2);
}
void
configureCCLs() {
    pinMode(CLKOUT, OUTPUT);
    pinMode(CLK960_2, OUTPUT);
    pinMode(CLK960_1, OUTPUT);
    // the goal is to allow clock signals to be properly routed dynamically at
    // startup
    Event0.set_generator(gen0::pin_pa7); // 20MHz
    Event1.set_generator(gen::ccl0_out); // 10MHz
    Event2.set_generator(gen::ccl2_out); // 5MHz
    Event0.set_user(user::ccl0_event_a);
    Event0.set_user(user::ccl1_event_a);
    Event1.set_user(user::ccl2_event_a);
    Event1.set_user(user::ccl3_event_a);
    if (digitalReadFast(ClockConfigurationBit) == LOW) {
        // configure for 20MHz
        configureCLK2(Event0); // PA7/CLKOUT
        configureCLK(Event1); // CCL0
        I960CLK2Rate = MicrocontrollerClockRate;
    } else {
        // configure for 10MHz
        configureCLK2(Event1); // PA3/CCL0 out
        configureCLK(Event2); // CCL2
        I960CLK2Rate = MicrocontrollerClockRate / 2;
    }
    I960CLKRate = I960CLK2Rate / 2;
    // generate a divide by two circuit
    Logic0.enable = true;
    Logic0.input0 = in::feedback;
    Logic0.input1 = in::disable;
    Logic0.input2 = in::disable;
    Logic0.output = out::disable;
    Logic0.truth = 0b0101'0101;
    Logic0.sequencer = sequencer::jk_flip_flop;
    Logic1.enable = true;
    Logic1.input0 = in::event_a;
    Logic1.input1 = in::disable;
    Logic1.input2 = in::disable;
    Logic1.output = out::disable;
    Logic1.sequencer = sequencer::disable;
    Logic1.truth = 0b0101'0101;
    // feed the result of the divide by two circuit into another divide by two
    // circuit
    Logic2.enable = true;
    Logic2.input0 = in::feedback;
    Logic2.input1 = in::disable;
    Logic2.input2 = in::event_a;
    Logic2.output = out::disable;
    Logic2.clocksource = clocksource::in2;
    Logic2.truth = 0b0101'0101;
    Logic2.sequencer = sequencer::jk_flip_flop;
    Logic3.enable = true;
    Logic3.input0 = in::event_a;
    Logic3.input1 = in::disable;
    Logic3.input2 = in::event_a;
    Logic3.output = out::disable;
    Logic3.clocksource = clocksource::in2;
    Logic3.sequencer = sequencer::disable;
    Logic3.truth = 0b0101'0101;
    Logic0.init();
    Logic1.init();
    Logic2.init();
    Logic3.init();
    Event0.start();
    Event1.start();
    Event2.start();
    Logic::start();
}
void
setupSystemClocks() {
    // as soon as possible, setup the 20MHz clock source
    CCP = 0xD8;
    // internal 20MHz oscillator + enable clkout
    CLKCTRL.MCLKCTRLA = 0b1000'0000;
    asm volatile ("nop");
    // make sure that the 20MHz clock runs in standby
    CCP = 0xD8;
    CLKCTRL.OSC20MCTRLA = 0b0000'0010;
    asm volatile ("nop");
}
void 
setup() {
    pinMode(ClockConfigurationBit, INPUT_PULLUP);
    // this function is super important for the execution of the system!
    setupSystemClocks();
    configureCCLs();
    EEPROM.begin();
    Wire.begin(0x09); 
    // setup other peripherals
    // then setup the serial port for now, I may disable this at some point
    Serial.begin(9600);
}



void 
loop() {
    delay(1000);
}

