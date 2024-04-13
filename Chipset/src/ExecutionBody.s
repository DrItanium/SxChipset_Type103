; i960SxChipset_Type103
; Copyright (c) 2022-2024, Joshua Scoggins
; All rights reserved.
; 
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;     * Redistributions of source code must retain the above copyright
;       notice, this list of conditions and the following disclaimer.
;     * Redistributions in binary form must reproduce the above copyright
;       notice, this list of conditions and the following disclaimer in the
;       documentation and/or other materials provided with the distribution.
; 
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
.file "ExecutionBody.s"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__RAMPZ__ = 0x3b
__tmp_reg__ = 0
__zero_reg__ = 1
; declarations
.macro DefinePort letter, base
PIN\letter = \base
DDR\letter = \base + 1
PORT\letter = \base + 2
BaseAddress\letter = \base
.endm
.macro DefinePinFunction func, port, index
\func\()Output = PORT\port
\func\()Input = PIN\port
\func\()Direction = DDR\port
\func\()BitIndex = \index
.if BaseAddress\port <= 0x3f
.macro toggleOutput\func
	sbi \func\()Output , \func\()BitIndex
.endm
.macro sbic_\func\()Output 
	sbic \func\()Output, \func\()BitIndex
.endm
.macro sbic_\func\()Input 
	sbic \func\()Input, \func\()BitIndex
.endm
.macro sbic_\func\()Direction 
	sbic \func\()Direction, \func\()BitIndex
.endm
.macro sbis_\func\()Output 
	sbis \func\()Output, \func\()BitIndex
.endm
.macro sbis_\func\()Input 
	sbis \func\()Input, \func\()BitIndex
.endm
.macro sbis_\func\()Direction 
	sbis \func\()Direction, \func\()BitIndex
.endm

.endif
.endm

.macro DefinePortFunction func, port
\func\()Output = PORT\port
\func\()Input = PIN\port
\func\()Direction = DDR\port
.endm


DefinePort A, 0x00
DefinePort B, 0x03
DefinePort C, 0x06
DefinePort D, 0x09
DefinePort E, 0x0c
DefinePort F, 0x0f
DefinePort G, 0x12
DefinePort H, 0x100
DefinePort J, 0x103
DefinePort K, 0x106
DefinePort L, 0x109
EIFR = 0x1c
GPIOR0 = 0x1e

DefinePinFunction Ready, E, 4
DefinePinFunction ADS, E, 5
DefinePinFunction HLDA, E, 6
DefinePinFunction IsIOOperation, E, 2
DefinePinFunction BE0, G, 3
DefinePinFunction BE1, G, 4
DefinePinFunction BLAST, G, 5
DefinePortFunction DataLinesLower, F
DefinePortFunction DataLinesUpper, C

; PD6 -> DEN
; PD7 -> WR
AddressLinesInterface = PINL 
MemoryWindowUpper = 0xfc
__snapshot__ = 9;
__high_data_byte960__ = 6
__low_data_byte960__ = 5
__rdy_signal_count_reg__ = 4
__eifr_mask_reg__ = 3
__direction_ff_reg__ = 2
.macro signalReady
	toggleOutputReady
.endm
.macro sbisrj a, b, dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbis \a, \b 	; 1 cycle if false, 2 cycles if true
	rjmp \dest		; 2 cycles
.endm
.macro sbicrj a, b, dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbic \a, \b		; 1 cycle if false, 2 cycles if true
	rjmp \dest		; 2 cycles
.endm
.macro computeTransactionWindow ; 2 cycles
	lds r28, AddressLinesInterface	; 2 cycles internal, 4 cycles external
.endm
.macro setupRegisterConstants
; use the lowest registers to make sure that we have Y, r16, and r17 free for usage
	ldi r16, lo8(112)
	mov __eifr_mask_reg__, r16
	ldi r16, lo8(-3)
	mov __rdy_signal_count_reg__, r16 ; load the ready signal amount
	ser r16
	mov __direction_ff_reg__, r16 ; 0xff
; setup index register Y to be the actual memory window, that way we can only update the lower half
	clr r28
	ldi r29, MemoryWindowUpper
.endm
.macro delay2cycles ; 2 cycles
	rjmp 1f
1:
.endm
.macro delay6cycles ; 6 cycles
	lpm ; tmp_reg is used implicity, who cares what we get back
	lpm ; tmp_reg is used implicity, who cares what we get back
.endm

.macro clearEIFR ; 1 cycle
	out EIFR, __eifr_mask_reg__ ; 1 cycle
.endm

.macro StoreToDataPort lo,hi ; 3 cycles
	out DataLinesLowerOutput, \lo ; 1 cycle
	out DataLinesUpperOutput, \lo ; 1 cycle
.endm

.macro WhenBlastIsLowGoto dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbisrj BLASTInput, BLASTBitIndex, \dest
.endm
.macro WhenBlastIsHighGoto dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbicrj BLASTInput, BLASTBitIndex, \dest
.endm
.macro getLowDataByte960_AVRGPIO  ; 1 cycle
	in __low_data_byte960__, PINF
.endm
.macro getHighDataByte960_AVRGPIO ; 2 cycles
	in __high_data_byte960__, PINC
.endm

.macro getLowDataByte960  ; 1 cycle
	getLowDataByte960_AVRGPIO
.endm
.macro getHighDataByte960 ; 2 cycles
	getHighDataByte960_AVRGPIO
.endm
.macro justWaitForTransaction
1: sbisrj EIFR, ADSBitIndex, 1b
clearEIFR
.endm
.macro waitForTransaction ; 3 cycles per iteration waiting, 2 cycles when condition met
	justWaitForTransaction
.endm
.macro SkipNextIfBE0High  ; 1 cycle when false, 2 cycles when true
	sbis PING, 3
.endm
.macro SkipNextIfBE1High  ; 1 cycle when false, 2 cycles when true
	sbis PING, 4
.endm
.macro setDataLinesDirection_AVRGPIO dir ; 3 cycles
	out DDRF, \dir ; 1 cycle
	sts DDRK, \dir ; 2 cycle
.endm
.macro setDataLinesDirection dir
	setDataLinesDirection_AVRGPIO \dir
.endm
.macro getDataWord960 ; 3 cycles
	getLowDataByte960 ; 1 cycle
	getHighDataByte960 ; 2 cycles
.endm
.macro StoreHighByteIfBE1Low offset ; 2 cycles when BE1 is HIGH, 5 cycles when BE1 is LOW
	SkipNextIfBE1High	; 1 cycle when false, 2 cycles when skipping
	std Y+\offset, __high_data_byte960__ ; 4 cycles
.endm
.macro FallthroughExecutionBody_WriteOperation
	signalReady 

	waitForTransaction
	sbisrj PINE, 5, .LXB_ShiftFromWriteToRead 
	sbisrj PINE, 2, .LXB_Write_DoIO_Nothing
	computeTransactionWindow
	getDataWord960
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   							; Yes, so store to the EBI
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	signalReady 												; first word down, onto the next one
	std Y+1,__high_data_byte960__								; Store the upper byte to the EBI
	delay2cycles												; wait for the next cycle to start
	WhenBlastIsHighGoto .L642                                   ; this is checking blast for the second set of 16-bits not the first
																; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getDataWord960
	StoreHighByteIfBE1Low 3
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.endm
.macro ReadBodyPrimary
	computeTransactionWindow
	ld __low_data_byte960__,Y
	ldd __high_data_byte960__,Y+1
	StoreToDataPort __low_data_byte960__,__high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart 
	signalReady 
	ldd __low_data_byte960__,Y+2
	ldd __high_data_byte960__,Y+3
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart 
	signalReady 
	ldd __low_data_byte960__,Y+4
	ldd __high_data_byte960__,Y+5
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd __low_data_byte960__,Y+6
	ldd __high_data_byte960__,Y+7
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd __low_data_byte960__,Y+8
	ldd __high_data_byte960__,Y+9
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd __low_data_byte960__,Y+10
	ldd __high_data_byte960__,Y+11
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd __low_data_byte960__,Y+12
	ldd __high_data_byte960__,Y+13
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd __low_data_byte960__,Y+14
	ldd __high_data_byte960__,Y+15
	StoreToDataPort __low_data_byte960__, __high_data_byte960__
.endm

.macro FallthroughExecution_ReadBody
	signalReady
	waitForTransaction 
	sbicrj PINE, 5, .LXB_ShiftFromReadToWrite
	sbisrj PINE, 2, .LXB_readOperation_CheckIO_Nothing
	ReadBodyPrimary
	rjmp .LXB_FirstSignalReady_ThenReadTransactionStart
.endm
.macro takeSnapshot
	in __snapshot__, PING
.endm
.macro sbrsrj a, b, dest 
	sbrs \a, \b
	rjmp \dest
.endm
.macro sbrcrj a, b, dest 
	sbrc \a, \b
	rjmp \dest
.endm
.global ExecutionBody
.global doIOReadOperation
.global doIOWriteOperation
.text

ExecutionBody:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
	setupRegisterConstants
	rjmp .LXB_ReadTransactionStart ; jump into the top of the invocation loop
.LXB_readOperation_CheckIO_Nothing:
	call doIOReadOperation			      ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart
.LXB_FirstSignalReady_ThenReadTransactionStart:
	signalReady
.LXB_ReadTransactionStart:
	waitForTransaction 
	sbicrj PINE, 5, .LXB_ShiftFromReadToWrite
	sbisrj PINE, 2, .LXB_readOperation_CheckIO_Nothing
	ReadBodyPrimary
	FallthroughExecution_ReadBody
.LXB_ShiftFromWriteToRead:
	setDataLinesDirection __direction_ff_reg__ ; change the direction to output
	sbisrj PINE, 2, .LXB_readOperation_CheckIO_Nothing
	ReadBodyPrimary
	FallthroughExecution_ReadBody
.LXB_Write_DoIO_Nothing:
	call doIOWriteOperation 			 ; perform the write operation
	rjmp .LXB_WriteTransactionStart		 ; restart execution

.LXB_SignalReady_ThenWriteTransactionStart:
	signalReady 
.LXB_WriteTransactionStart:
	waitForTransaction
	sbisrj PINE, 5, .LXB_ShiftFromWriteToRead 
	sbisrj PINE, 2, .LXB_Write_DoIO_Nothing
	computeTransactionWindow
	getDataWord960
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   							; Yes, so store to the EBI
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	signalReady 												; first word down, onto the next one
	std Y+1,__high_data_byte960__								; Store the upper byte to the EBI
	delay2cycles												; wait for the next cycle to start
	WhenBlastIsHighGoto .L642                                   ; this is checking blast for the second set of 16-bits not the first
																; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getDataWord960
	StoreHighByteIfBE1Low 3
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	FallthroughExecutionBody_WriteOperation
.L642:
	getDataWord960				
	signalReady 				  ; start the next word signal and we can use the 6 cycles to get ready
	std Y+2,__low_data_byte960__  ; use the time to store into memory while the ready signal counter is doing its thing
	std Y+3,__high_data_byte960__ ; use the time to store into memory while the ready signal counter is doing its thing
	WhenBlastIsLowGoto .LXB_WriteBytes4_and_5_End	; We can now safely check if we should terminate execution
	getDataWord960				
	signalReady					  ; Start the process for the next word ( at this point we will be at a 64-bit number once this ready goes through)
	std Y+4,__low_data_byte960__  ; save the word (bits 32-39)
	std Y+5,__high_data_byte960__ ; save the word (bits 40-47)
	WhenBlastIsHighGoto .L649	  ; we have more data to transfer
	getDataWord960				
	StoreHighByteIfBE1Low 7
	std Y+6,__low_data_byte960__  ; always save the lower byte since we flowed into here
	FallthroughExecutionBody_WriteOperation
.L649:
	getDataWord960
	signalReady
	std Y+6,__low_data_byte960__
	std Y+7,__high_data_byte960__
	WhenBlastIsLowGoto .LXB_WriteBytes8_and_9_End
	getDataWord960
	signalReady
	std Y+8,__low_data_byte960__
	std Y+9,__high_data_byte960__
	WhenBlastIsHighGoto .L657
	getDataWord960
	StoreHighByteIfBE1Low 11
	std Y+10,__low_data_byte960__
	FallthroughExecutionBody_WriteOperation
.L657:
	getDataWord960
	signalReady
	std Y+10,__low_data_byte960__
	std Y+11,__high_data_byte960__
	WhenBlastIsHighGoto .L661
	getDataWord960
	StoreHighByteIfBE1Low 13
	std Y+12,__low_data_byte960__
	FallthroughExecutionBody_WriteOperation
.L661:
	getDataWord960
	signalReady
	std Y+12,__low_data_byte960__
	std Y+13,__high_data_byte960__
	getDataWord960
	StoreHighByteIfBE1Low 15
	std Y+14,__low_data_byte960__
	FallthroughExecutionBody_WriteOperation
.LXB_WriteBytes4_and_5_End:
	getDataWord960
	StoreHighByteIfBE1Low 5
	std Y+4,__low_data_byte960__
	FallthroughExecutionBody_WriteOperation
.LXB_WriteBytes8_and_9_End:
	getDataWord960
	StoreHighByteIfBE1Low 9
	std Y+8,__low_data_byte960__
	FallthroughExecutionBody_WriteOperation
.LXB_do16BitWriteOperation:
	StoreHighByteIfBE1Low 1
	FallthroughExecutionBody_WriteOperation

.LXB_ShiftFromReadToWrite:
; start setting up for a write operation here
	setDataLinesDirection __zero_reg__ 	
	sbisrj PINE, 2, .LXB_Write_DoIO_Nothing
	computeTransactionWindow
	getDataWord960
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   ; Yes, so store to the EBI
	WhenBlastIsLowGoto	.LXB_do16BitWriteOperation
	signalReady 											
	std Y+1,__high_data_byte960__												; Store the upper byte to the EBI
	delay2cycles
	WhenBlastIsHighGoto .L642 ; this is checking blast for the second set of 16-bits not the first
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getDataWord960
	StoreHighByteIfBE1Low 3
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	FallthroughExecutionBody_WriteOperation

; the idea scenario is to not load or store to SRAM if I don't need to. The
; problem is that I have to have access to the data ahead of time to make this determination which
; defeats the point completely. It does save four avr cycles each time we can eliminate this overhead

; The only other place to eliminate overhead has to do with the spinup time. 
; I am wondering if there is some sort of way to encode system state into the shape of the code 
; itself... I do this currently with reads and writes to eliminate the overhead
; of switching the direction registers constantly. 

; If I go back to the DEN pin then I can use that safely as long as I make sure that I don't sample 
; DEN too quickly at the end of a transaction!

; Then I do not need to clear EIFR
