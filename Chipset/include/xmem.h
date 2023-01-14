/*
 * xmem.cpp
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 */
#ifndef __89089DA1_BAAC_497C_8E1FFEF0911A6844
#define __89089DA1_BAAC_497C_8E1FFEF0911A6844

#include <stdlib.h>
#include <stdint.h>

namespace xmem {
    /*
	 * State variables used by the heap
	 */

	struct heapState {
        char *__malloc_heap_start;
        char *__malloc_heap_end;
        void *__brkval;
        void *__flp;
	};

    /*
     * Results of a self-test run
     */

    struct SelfTestResults {
        bool succeeded;
        volatile uint8_t *failedAddress;
        uint8_t failedBank;
        uint8_t expectedValue;
        uint8_t gotValue;
    };

    /*
     * Prototypes for the management functions
     */

    void begin(bool heapInXmem_);
    void setMemoryBank(uint8_t bank_,bool switchHeap_=true);
    SelfTestResults selfTest();
    uint8_t getCurrentMemoryBank() noexcept;
    void saveHeap(uint8_t bank_);
    void restoreHeap(uint8_t bank_);
    uintptr_t getStartAddress(uint8_t bank) noexcept;
    uintptr_t getEndAddress(uint8_t bank) noexcept;
}

/*
 * References to the private heap variables
 */
extern "C" {
	extern void *__flp;
	extern void *__brkval;
}

#endif
