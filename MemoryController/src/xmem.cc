/*
 * xmem.cpp
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 *
 *  Modified by Rugged Circuits LLC (25 Oct 2011) for use with the QuadRAM 512k shield:
 *     http://ruggedcircuits.com/html/quadram.html
 *  
 *  Version 1.2:
 *  -----------
 *     * Fixed reference to 'bank_' in setMemoryBank().
 *     * Added include for Arduino.h/WProgram.h if using Arduino IDE. 
 *     (Contributed by Adam Watson - adam@adamlwatson.com)
 *
 *  Version 1.1:
 *  -----------
 *     * begin() function modified to also set __brkval to the RAM start so it doesn't grow into the stack
 *       (thanks to Gene Reeves).
 *
 */

// Select which shield you are using, Andy Brown's or the Rugged Circuits QuadRAM Shield.
// Only uncomment one of the two lines below.


#include <avr/io.h>
#include "xmem.h"
#ifdef I960_MEGA_MEMORY_CONTROLLER
#include "BankSelection.h"
#endif

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif


namespace xmem {
    namespace {

        constexpr auto getNumberOfBankHeapStates() noexcept {
            return 16 + 64;
        }
        constexpr auto getLastAddress() noexcept {
            return RAMEND + 0x8000;
        }
        constexpr auto getFirstAddress() noexcept {
            return RAMEND + 1;
        }
        static_assert(getLastAddress() == 0xA1FF);

        /**
         * @brief state for each bank
         */
	    heapState bankHeapStates[getNumberOfBankHeapStates()];
        /*
         * The currently selected bank
         */
        volatile uint8_t currentBank = 0;
    }


	/*
	 * Initial setup. You must call this once
	 */

	void begin(bool heapInXmem_) {

		// initialise the heap states

        // set up the xmem registers
        XMCRB=0b00000'001; // need 32k. one pin released
        XMCRA=0b1'100'00'00; // put in one cycle wait states
        pinMode(RealA15, OUTPUT);
        InternalBus::begin();
        External328Bus::begin();
        InternalBus::select();
        // we have two spaces on the memory controller
        // internal bus is mapped to the lower 32k of the memory space
        // the 32/8 bus is mapped to the upper 32k of the memory space
        // 
        // We map 32k/24k chunks in each bank. The first 16 banks are mapped to
        // the internal 24k memory chunks. The remaning 64 banks are mapped to
        // the external 2 megabytes of ram found on the 328 bus
        // 
        // I am making it a 32k window so that we can get a full 32k out of
        // each section. That gives us exactly 2.5 megabytes of ram!
        //
        // This change also prevents shenanigans when dealing with the two pool
        // sets
        if (heapInXmem_) {
            __malloc_heap_end = reinterpret_cast<char*>(getLastAddress());
            __malloc_heap_start = reinterpret_cast<char*>(getFirstAddress());
            __brkval= reinterpret_cast<char*>(getFirstAddress());
        }
        for (uint8_t i = 0; i < getNumberOfBankHeapStates(); ++i) {
            saveHeap(i);
        }
        setMemoryBank(0);
	}
	/*
	 * Set the memory bank
	 */

	void setMemoryBank(uint8_t bank_,bool switchHeap_) {
		// check
        Serial.print(F("Switching to bank: "));
        Serial.println(bank_);
		if(bank_==currentBank)
			return;

		// save heap state if requested

		if(switchHeap_)
			saveHeap(currentBank);

		// switch in the new bank

        if (bank_ < 16) {
            InternalBus::setBank(bank_);
            InternalBus::select();
        } else {
            External328Bus::setBank(bank_ - 16);
            External328Bus::select();
        }
		// save state and restore the malloc settings for this bank

		currentBank=bank_;

		if(switchHeap_)
			restoreHeap(currentBank);
	}

	/*
	 * Save the heap variables
	 */

	void saveHeap(uint8_t bank_) {
		bankHeapStates[bank_].__malloc_heap_start=__malloc_heap_start;
		bankHeapStates[bank_].__malloc_heap_end=__malloc_heap_end;
		bankHeapStates[bank_].__brkval=__brkval;
		bankHeapStates[bank_].__flp=__flp;
	}

	/*
	 * Restore the heap variables
	 */

	void restoreHeap(uint8_t bank_) {
		__malloc_heap_start=bankHeapStates[bank_].__malloc_heap_start;
		__malloc_heap_end=bankHeapStates[bank_].__malloc_heap_end;
		__brkval=bankHeapStates[bank_].__brkval;
		__flp=bankHeapStates[bank_].__flp;
	}
	/*
	 * Self test the memory. This will destroy the entire content of all
	 * memory banks so don't use it except in a test scenario.
	 */
	SelfTestResults selfTest() {
        auto getStart = [](uint8_t bank) { return getLastAddress(); };
        auto getEnd = [](uint8_t bank) { return getFirstAddress(); };
		volatile uint8_t *ptr;
		SelfTestResults results;

		// write an ascending sequence of 1..237 running through
		// all memory banks

		auto writeValue=1;
		for(auto bank=0;bank<getNumberOfBankHeapStates();bank++) {

			setMemoryBank(bank);
            auto startPtr = getStart(bank);
            auto stopPtr = getEnd(bank);
			for(ptr=reinterpret_cast<uint8_t*>(startPtr); ptr >= reinterpret_cast<uint8_t*>(stopPtr); --ptr) {
				*ptr=writeValue;

				if(writeValue++==237)
					writeValue=1;
			}
		}

		// verify the writes

		writeValue=1;
		for(auto bank=0;bank<getNumberOfBankHeapStates();bank++) {

			setMemoryBank(bank);
            auto startPtr = getStart(bank);
            auto stopPtr = getEnd(bank);
			for(ptr=reinterpret_cast<uint8_t*>(startPtr); ptr >= reinterpret_cast<uint8_t*>(stopPtr); --ptr) {
				auto readValue=*ptr;

				if(readValue!=writeValue) {
					results.succeeded=false;
					results.failedAddress=ptr;
					results.failedBank=bank;
                    results.expectedValue = writeValue;
                    results.gotValue = readValue;
					return results;
				}

				if(writeValue++==237)
					writeValue=1;
			}
		}

		results.succeeded=true;
		return results;
	}

    uint8_t 
    getCurrentMemoryBank() noexcept {
        return currentBank; 
    }

    uintptr_t 
    getStartAddress(uint8_t) noexcept {
        return getFirstAddress();
    }
    uintptr_t 
    getEndAddress(uint8_t) noexcept {
        return getLastAddress();
    }
}
