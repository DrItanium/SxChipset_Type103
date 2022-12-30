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

#ifndef SXCHIPSET_TYPE103_DETECT_H__
#define SXCHIPSET_TYPE103_DETECT_H__
#include <stdint.h>
#ifdef __AVR__
#include <avr/io.h>
#endif
#if defined(SPDR) && defined(SPIF) && defined(SPSR)
#define AVR_SPI_AVAILABLE
#endif

#ifndef GPIOR0
extern uint8_t GPIOR0;
#endif
#ifndef GPIOR1
extern uint8_t GPIOR1;
#endif
#ifndef GPIOR2
extern uint8_t GPIOR2;
#endif

constexpr uint32_t getCPUFrequency() noexcept {
#ifdef F_CPU
    return F_CPU;
#else
    return 0;
#endif
}

constexpr auto getRAMStart() noexcept {
#ifdef RAMSTART
    return RAMSTART;
#else
    return 0;
#endif
}

constexpr auto getRAMEnd() noexcept {
#ifdef RAMEND
    return RAMEND;
#else
    return 0;
#endif
}

constexpr auto getXMEMSize() noexcept {
#ifdef XRAMSIZE
    return XRAMSIZE;
#else
    return 0;
#endif
}

constexpr auto getXMEMEnd() noexcept {
#ifdef XRAMEND
    return XRAMEND;
#else
        return 0;
#endif
}

#ifdef BOARD_TYPE
#  if BOARD_TYPE == 103
#    define TYPE103_BOARD
#  elif BOARD_TYPE == 200
#    define TYPE200_BOARD
#  else
#    error "Unknown Board type defined!"
#  endif
#else
#  error "No Board Type defined!"
#endif


#endif // end SXCHIPSET_TYPE103_DETECT_H__
