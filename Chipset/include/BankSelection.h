/**
* i960SxChipset_Type103
* Copyright (c) 2022-2023, Joshua Scoggins
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

#ifndef BANK_SELECTION_H__
#define BANK_SELECTION_H__
#include <stdint.h>
#include <Arduino.h>
#include "Types.h"
#include "Pinout.h"

class BankSwitcher {
public:
    BankSwitcher() = delete;
    ~BankSwitcher() = delete;
    BankSwitcher(const BankSwitcher&) = delete;
    BankSwitcher(BankSwitcher&&) = delete;
    BankSwitcher& operator=(const BankSwitcher&) = delete;
    BankSwitcher& operator=(BankSwitcher&&) = delete;
    static void begin() noexcept;
    static void setBank(uint24_t bank) noexcept;
    static void setBank(const SplitWord32& bank) noexcept;
    static auto getBank() noexcept { return currentBank_; }
private:
    static inline uint24_t currentBank_ = 0;
};
/**
 * @brief Saves the current bank on constructor, it is restored when this object goes out of scope (RAII style); You cannot copy or move this but you can pass it by reference!
 */
struct BankSaver final {
    BankSaver() noexcept : theBank_(BankSwitcher::getBank()) { }
    ~BankSaver() noexcept { BankSwitcher::setBank(theBank_); }
    // cannot copy or move it but still can be passed around as a token
    BankSaver(const BankSaver&) = delete;
    BankSaver(BankSaver&&) = delete;
    BankSaver& operator=(const BankSaver&) = delete;
    BankSaver& operator=(BankSaver&&) = delete;
private:
    uint24_t theBank_;
};
#endif
