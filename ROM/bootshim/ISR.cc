//
// Created by jwscoggins on 3/27/21.
//
#include "../cortex/IODevice.h"
#include "../cortex/Interrupts.h"
extern "C" void ISR0(void);
extern "C" void SystemCounterISR(void);
extern "C" void ISR_NMI(void);
void IncrementSystemCounter();

extern "C"
void
ISR0(void) {
    InterruptFunction fn = getISR0Function();
    if (fn) {
        fn();
    }
}
extern "C"
void
SystemCounterISR(void) {
    IncrementSystemCounter();
}
extern "C"
void
ISR_NMI(void) {
    InterruptFunction fn = getNMIFunction();
    if (fn) {
        fn();
    }
}
