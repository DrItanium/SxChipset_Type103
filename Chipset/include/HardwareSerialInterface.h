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
#ifndef I960_HARDWARE_SERIAL_INTERFACE__
#define I960_HARDWARE_SERIAL_INTERFACE__
#include <Arduino.h>
class HardwareSerialInterface {
    public:
        HardwareSerialInterface(HardwareSerial& ser) : _interface(ser) { }
        const auto& getInterface() const noexcept { return _interface; }
        auto& getInterface() noexcept { return _interface; }
        void setBaud(uint32_t value) noexcept { _baud = value; }
        constexpr auto getBaud() const noexcept { return _baud; }
        void setConfig(uint8_t value) noexcept { _cfg = value; }
        constexpr auto getConfig() const noexcept { return _cfg; }
        void 
        begin(uint32_t baud, uint8_t cfg) noexcept {
            setBaud(baud);
            setConfig(cfg);
            begin();
        }
        void
        begin() noexcept {
            _interface.begin(_baud, _cfg);
        }

    private:
        HardwareSerial& _interface;
        uint32_t _baud;
        uint8_t _cfg = SERIAL_8N1;
};

#endif // end I960_HARDWARE_SERIAL_INTERFACE__
