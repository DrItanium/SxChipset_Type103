/*
i960SxChipset_Type103
Copyright (c) 2023, Joshua Scoggins
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
#include "Pinout.h"
namespace {
    SPIRequest currentRequest;
}
// taken from Tom Almy's book "Still Far Inside The Arduino" and adapted for my
// purposes
ISR(SPI_STC_vect) {
    if (currentRequest.input) {
        *currentRequest.input++ = SPDR;
    }
    if (--currentRequest.count > 0) {
        SPDR = (currentRequest.output != nullptr ? *currentRequest.output++ : 0);
    } else {
        currentRequest.active = false;
        raiseInterrupt(currentRequest.interruptNumber);

    }
}

bool
spiAvailable() noexcept {
    return !currentRequest.active;
}

void 
runSPI(uint8_t* input, uint8_t* output, uint8_t count, OnSPIFinishedCallback callback) {
}
