/**
 *
 *
 * i960SxChipset_Type103
 * Copyright (c) 2022, Joshua Scoggins
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

struct TreatAsLowerDecode { };
struct TreatAsCtlPlusAddrUpper { };
union Word8{
    uint8_t whole;
    struct {
        // reversed because of the output ordering
        uint8_t b7 : 1;
        uint8_t b6 : 1;
        uint8_t b5 : 1;
        uint8_t b4 : 1;
        uint8_t b3 : 1;
        uint8_t b2 : 1;
        uint8_t b1 : 1;
        uint8_t b0 : 1;
    } bits;
};
union DecodeRoms {
    uint32_t whole : 19;
    struct {
        uint32_t a1 : 1;
        uint32_t a2 : 1;
        uint32_t a3 : 1;
        uint32_t a4 : 1;
        uint32_t a5 : 1;
        uint32_t a6 : 1;
        uint32_t a7 : 1;
        uint32_t a8 : 1;
        uint32_t xio23 : 2;
        uint32_t wr : 1;
        uint32_t sel : 1;
        uint32_t a9 : 1;
        uint32_t a15 : 1;
        uint32_t a14 : 1;
        uint32_t a10 : 1;
        uint32_t a11 : 1;
        uint32_t a13 : 1;
        uint32_t a12 : 1;
    } addrLower;
    struct {
        uint32_t be1 : 1;
        uint32_t be0 : 1;
        uint32_t data_int1 : 1;
        uint32_t data_int0 : 1;
        uint32_t xio4 : 1;
        uint32_t xio5 : 1;
        uint32_t a16 : 1;
        uint32_t a17 : 1;
        uint32_t sel : 1;
        uint32_t den : 1;
        uint32_t blast : 1;
        uint32_t fail : 1;
        uint32_t a18 : 1;
        uint32_t addr_hi : 1;
        uint32_t a23 : 1;
        uint32_t a19 : 1;
        uint32_t a20 : 1;
        uint32_t a22 : 1;
        uint32_t a21 : 1;
    } ctlPlusAddrUpper;

    [[nodiscard]] uint8_t generate(TreatAsLowerDecode) const noexcept {
        Word8 result;
        result.whole = 0;
        if (addrLower.sel == 0) {
            result.bits.b0 = addrLower.wr;
            result.bits.b1 = addrLower.a1;
            result.bits.b2 = addrLower.a2;
            result.bits.b3 = addrLower.a3;
            result.bits.b4 = addrLower.a4;
            result.bits.b5 = addrLower.a5;
            result.bits.b6 = addrLower.a6;
            result.bits.b7 = addrLower.a7;
        } else {
            result.bits.b0 = addrLower.a8;
            result.bits.b1 = addrLower.a9;
            result.bits.b2 = addrLower.a10;
            result.bits.b3 = addrLower.a11;
            result.bits.b4 = addrLower.a12;
            result.bits.b5 = addrLower.a13;
            result.bits.b6 = addrLower.a14;
            result.bits.b7 = addrLower.a15;
        }
        return result.whole;
    }
    [[nodiscard]] uint8_t generate(TreatAsCtlPlusAddrUpper) const noexcept {
        Word8 result;
        result.whole = 0;
        if (addrLower.sel == 0) {
            result.bits.b0 = ctlPlusAddrUpper.be0;
            result.bits.b1 = ctlPlusAddrUpper.be1;
            result.bits.b2 = ctlPlusAddrUpper.blast;
            result.bits.b3 = ctlPlusAddrUpper.den;
            result.bits.b4 = ctlPlusAddrUpper.fail;
            result.bits.b5 = ctlPlusAddrUpper.data_int0;
            result.bits.b6 = ctlPlusAddrUpper.data_int1;
            result.bits.b7 = ctlPlusAddrUpper.addr_hi;
        } else {
            result.bits.b0 = ctlPlusAddrUpper.a16;
            result.bits.b1 = ctlPlusAddrUpper.a17;
            result.bits.b2 = ctlPlusAddrUpper.a18;
            result.bits.b3 = ctlPlusAddrUpper.a19;
            result.bits.b4 = ctlPlusAddrUpper.a20;
            result.bits.b5 = ctlPlusAddrUpper.a21;
            result.bits.b6 = ctlPlusAddrUpper.a22;
            result.bits.b7 = ctlPlusAddrUpper.a23;
        }
        return result.whole;
    }
};
template<typename T>
bool generateROM(std::string path) noexcept {
    std::fstream rom(path, rom.binary | rom.trunc | rom.in | rom.out);
    if (!rom.is_open()) {
        std::cout << "Failed to open " << path << std::endl;
        return false;
    }
    for (uint32_t i = 0; i < 0b0111'1111'1111'1111'1111; ++i) {
        DecodeRoms curr;
        curr.whole = i;
        rom << curr.generate(T{});
    }
    rom.close();
    return true;
}
int main() {
    if (!generateROM<TreatAsLowerDecode>("u3.bin")) {
        return 1;
    }
    if (!generateROM<TreatAsCtlPlusAddrUpper>("u10.bin")) {
        return 1;
    }
    return 0;
}
