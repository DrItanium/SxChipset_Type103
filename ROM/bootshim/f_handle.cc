/*
i960SxChipset
Copyright (c) 2020-2021, Joshua Scoggins
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
/*
 * Fault handler routines
 */
#include "../cortex/IODevice.h"
#include "../cortex/Faults.h"
#include <string>
void
basicDisplay(const std::string& kind, cortex::FaultData* record) {
    cortex::ChipsetBasicFunctions::Console::write(kind.c_str());
    cortex::ChipsetBasicFunctions::Console::writeLine(" FAULT RAISED!");
    record->display();
}
inline void
basicOperation(const std::string& kind, cortex::FaultData* record, cortex::FaultHandler handler) {
    if (handler)  {
        handler(record);
    } else {
        basicDisplay(kind, record);
    }
}

extern "C"
void
user_reserved(cortex::FaultData* record) {
    basicOperation("USER RESERVED", record, cortex::getUserReservedFaultHandler());
}

extern "C"
void
user_trace(cortex::FaultData* record) {
    basicOperation("USER TRACE", record, cortex::getUserTraceFaultHandler());
}

extern "C"
void
user_operation(cortex::FaultData* record) {
    basicOperation("USER OPERATION", record, cortex::getUserOperationFaultHandler());
}
extern "C"
void
user_arithmetic(cortex::FaultData* record) {
    basicOperation("USER ARITHMETIC", record, cortex::getUserArithmeticFaultHandler());
}
extern "C"
void
user_real_arithmetic(cortex::FaultData* record) {
    basicOperation("USER REAL ARITHMETIC", record, cortex::getUserRealArithmeticFaultHandler());
}
extern "C"
void
user_constraint(cortex::FaultData* record) {
    basicOperation("USER CONSTRAINT", record, cortex::getUserConstraintFaultHandler());
}
extern "C"
void
user_protection(cortex::FaultData* record) {
    basicOperation("USER PROTECTION", record, cortex::getUserProtectionFaultHandler());
}
extern "C"
void
user_machine(cortex::FaultData* record) {
    basicOperation("USER MACHINE", record, cortex::getUserMachineFaultHandler());
}
extern "C"
void
user_type(cortex::FaultData* record) {
    basicOperation("USER TYPE", record, cortex::getUserTypeFaultHandler());
}
