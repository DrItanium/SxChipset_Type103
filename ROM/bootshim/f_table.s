/*
i960SxChipset_Type103
Copyright (c) 2020-2023, Joshua Scoggins
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
/* fault table */
	.include "macros.s"
    .globl  fault_table
    .align  8
.macro ReservedFaultEntry 
	FaultTableEntry_TraceTableEntry 0
.endm
fault_table:
	ReservedFaultEntry # reserved fault
	FaultTableEntry_TraceTableEntry 1 # trace fault
	FaultTableEntry_TraceTableEntry 2 # operation fault
	FaultTableEntry_TraceTableEntry 3 # arithmetic fault
	FaultTableEntry_TraceTableEntry 4 # floating point fault
	FaultTableEntry_TraceTableEntry 5 # constraint fault
	ReservedFaultEntry 
	FaultTableEntry_TraceTableEntry 6 # protection fault
	FaultTableEntry_TraceTableEntry 7 # machine fault
	ReservedFaultEntry
	FaultTableEntry_TraceTableEntry 8 # type fault
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
	ReservedFaultEntry
