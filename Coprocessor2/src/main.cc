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
enum class CPUClockSpeed {
    MHz_20,
    MHz_10,
};
constexpr auto CLKOUT = PIN_PA7;
constexpr auto CLK10 = PIN_PA3;
constexpr auto CLK5 = PIN_PD3;
constexpr auto CLK960_2 = PIN_PA2;
constexpr auto CLK960_1 = PIN_PC2;
void
setupSystemClocks(CPUClockSpeed speed) {
    // event 0 and 2 handle 10MHz generation, we want this in all cases so the
    // goal is to provide a way to configure the event system dynamically to
    // route to PA2 for the CLK2 signal and PC2 for the CLK signal
    Event0.set_generator(gen0::pin_pa7);
    Event0.set_user(user::ccl0_event_a);
    Event0.set_user(user::ccl1_event_a);
    Event1.set_generator(gen::ccl0_out);
    Event1.set_user(user::ccl2_event_a);
    Event1.set_user(user::ccl3_event_a);
    Event2.set_generator(gen::ccl2_out);
    Event3.set_generator(gen::ccl1_out);
    switch (speed) {
        case CPUClockSpeed::MHz_10:
            Event1.set_user(user::evouta_pin_pa2); // pa3 is routed to pa2 via event1
            Event2.set_user(user::evoutc_pin_pc2); // ccl2 output is routed to pc2 via event2
            break;
        case CPUClockSpeed::MHz_20:
            Event3.set_user(user::evouta_pin_pa2); // pa7 is routed to pa2 via event3
            Event1.set_user(user::evoutc_pin_pc2); // ccl0 is routed to pc2 via event1
            break;
        default:
            break;
    }
    Event0.start();
    Event1.start();
    Event2.start();
    Event3.start();
    Logic0.enable = true;
    Logic0.input0 = in::feedback;
    Logic0.input1 = in::disable;
    Logic0.input2 = in::disable;
    Logic0.output = out::enable;
    Logic0.truth = 0b0101'0101;
    Logic0.sequencer = sequencer::jk_flip_flop;
    Logic1.enable = true;
    Logic1.input0 = in::event_a;
    Logic1.input1 = in::disable;
    Logic1.input2 = in::disable;
    Logic1.output = out::disable;
    Logic1.sequencer = sequencer::disable;
    Logic1.truth = 0b0101'0101;
    Logic2.enable = true;
    Logic2.input0 = in::feedback;
    Logic2.input1 = in::disable;
    Logic2.input2 = in::event_a;
    Logic2.output = out::enable;
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

}
void 
setup() {
    // as soon as possible, setup the 20MHz clock source
    CCP = 0xD8;
    // internal 20MHz oscillator + enable clkout
    CLKCTRL.MCLKCTRLA = 0b1000'0000;
    asm volatile ("nop");
    // make sure that the 20MHz clock runs in standby
    CCP = 0xD8;
    CLKCTRL.OSC20MCTRLA = 0b0000'0010;
    asm volatile ("nop");
    pinMode(CLKOUT, OUTPUT);
    pinMode(CLK10, OUTPUT);
    pinMode(CLK5, OUTPUT);
    pinMode(CLK960_2, OUTPUT);
    pinMode(CLK960_1, OUTPUT);
    setupSystemClocks(CPUClockSpeed::MHz_10);
    Logic::start();
    // then setup the serial port for now, I may disable this at some point
    Serial1.swap(1);
    Serial1.begin(9600);
}



void 
loop() {
    Serial1.println("Donuts");
    delay(1000);
}

