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
PINE = 0x0C
PINF = 0x0F
DDRF = 0x10
PORTF = 0x11
PING = 0x12
PINK = 262
PINL = 0x109
PINJ = 0x103
DDRK = 263
PORTK = 264
EIFR = 0x1c
InternalAddressLinesInterface = PINL
ExternalAddressLinesInterface = 0x2300
ExternalDataLinesInterface = 0x2308
AddressLinesInterface = InternalAddressLinesInterface
MemoryWindowUpper = 0x22
TCNT2 = 0xb2
__highest_data_byte960__ = 8
__higher_data_byte960__ = 7
__high_data_byte960__ = 6
__low_data_byte960__ = 5
__rdy_signal_count_reg__ = 4
__eifr_mask_reg__ = 3
__direction_ff_reg__ = 2
.macro signalReady ; 2 cycles itself, 6 cycles after that before next part of transaction
	sts TCNT2, __rdy_signal_count_reg__  ; this one is special because the store takes two cycles itself
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
.macro DirectDataLines_StoreToDataPort lo,hi
	out PORTF, \lo 	; 1 cycle
	sts PORTK, \hi	; 2 cycles
.endm
.macro StoreToDataPort lo,hi ; 3 cycles
DirectDataLines_StoreToDataPort \lo, \hi
.endm
.macro WhenBlastIsLowGoto dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbisrj PING, 5, \dest
.endm
.macro WhenBlastIsHighGoto dest ; 3 cycles when branch taken, 2 cycles when skipped
	sbicrj PING, 5, \dest
.endm
.macro getLowDataByte960  ; 1 cycle
	in __low_data_byte960__, PINF
.endm
.macro getHighDataByte960 ; 2 cycles
	lds __high_data_byte960__, PINK
.endm
.macro waitForTransaction ; 3 cycles per iteration waiting, 2 cycles when condition met
1: sbisrj EIFR, 4, 1b
.endm
.macro SkipNextIfBE0High  ; 1 cycle when false, 2 cycles when true
	sbis PING, 3
.endm
.macro SkipNextIfBE1High  ; 1 cycle when false, 2 cycles when true
	sbis PING, 4
.endm
.macro DirectDataLines_SetDirection dir
	out DDRF, \dir ; 1 cycle
	sts DDRK, \dir ; 2 cycle
.endm
.macro setDataLinesDirection dir ; 3 cycles
	DirectDataLines_SetDirection \dir
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
	sbisrj EIFR,5, .LXB_ShiftFromWriteToRead 
	clearEIFR
	sbicrj PINE, 6, .LXB_Write_DoIO_Nothing
	computeTransactionWindow
	getLowDataByte960                  							; Load lower byte from
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   							; Yes, so store to the EBI
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	getHighDataByte960                 							; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
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
.macro DoNothingResponseLoop
1: signalReady
   delay6cycles
   WhenBlastIsHighGoto 1b
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
	sbicrj PINE, 2, 1f 
	call doIOReadOperation			      ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart		  ; And we are done :)
1:
	StoreToDataPort __zero_reg__, __zero_reg__ ; make sure we don't leak previous state because this is a read from an open bus area
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart ; if BLAST is low then we are done and just return
	DoNothingResponseLoop
.LXB_FirstSignalReady_ThenReadTransactionStart:
	signalReady
.LXB_ReadTransactionStart:
	waitForTransaction 										; minimum is 2 cycles for the skip
	sbicrj EIFR,5, .LXB_ShiftFromReadToWrite 				; 2 cycles skipped, 3 cycles taken
.LXB_PrimaryReadTransaction:
	clearEIFR						; Waiting for next memory transaction (1 cycle avr)
	sbicrj PINE, 6, .LXB_readOperation_CheckIO_Nothing		   ; 2 cycles (avr) if we skip over, 3 cycles if we take the operation
	computeTransactionWindow								   ; 2 cycles (avr)
	ld __low_data_byte960__,Y 		 						   ; 4 cycles (avr)
	ldd __high_data_byte960__,Y+1   						   ; 4 cycles (avr)
	StoreToDataPort __low_data_byte960__,__high_data_byte960__ ; 3 cycles (avr)
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart ; 3 cycles when done, 2 cycles when needing to continue (avr)
																	  ; if we are done here then the total cycles is 23 cycles => 1150 ns
	signalReady 											   ; 2 cycles (avr) but also need to wait 6 cycles before the next part of the transaction starts
	ldd __low_data_byte960__,Y+2							   ; 4 cycles (avr)
	ldd __high_data_byte960__,Y+3							   ; 4 cycles (avr) (mid way through this operation ready has been signaled) so
															   ; 	2 (avr) cycles first transaction
															   ; Total cycles first transaction: 20 + 2 + 4 + 2 + 2 => 30 cycles (avr) => 
															   ; 	2 (avr) cycles second transaction from the load
	StoreToDataPort __low_data_byte960__, __high_data_byte960__;    3 (avr) cycles 
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart ; 3 cycles when done, 2 cycles when needing to continue (avr)
																	  ; when done we are at 8 cycles for this part and a total of 38 cycles (avr)
	signalReady 													  ; 2 cycles but there is a 6 cycle delay
	ldd __low_data_byte960__,Y+4									  ; 4 cycles
	ldd __high_data_byte960__,Y+5									  ; 2 cycles before ready
																	  ; total cycles second transaction: 15 cycles (avr), 750ns (avr), this is mostly correct with what I see through the scope
																	  ; total cycles so far: 45 cycles (avr) 
																	  ; 2 cycles after into the next transaction
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
	rjmp .LXB_FirstSignalReady_ThenReadTransactionStart
.LXB_ShiftFromWriteToRead:
	setDataLinesDirection __direction_ff_reg__ ; change the direction to output
	clearEIFR						      ; Waiting for next memory transaction
	sbicrj PINE, 6, .LXB_readOperation_CheckIO_Nothing
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
	rjmp .LXB_FirstSignalReady_ThenReadTransactionStart

.LXB_Write_DoIO_Nothing:
	sbicrj PINE, 2, 1f
	call doIOWriteOperation 			 ; perform the write operation
	rjmp .LXB_WriteTransactionStart		 ; restart execution
1:
	WhenBlastIsLowGoto .LXB_SignalReady_ThenWriteTransactionStart ; if BLAST is low then we are done and just return
	DoNothingResponseLoop
.LXB_SignalReady_ThenWriteTransactionStart:
	signalReady 
.LXB_WriteTransactionStart:
	waitForTransaction
	sbisrj EIFR,5, .LXB_ShiftFromWriteToRead 
	clearEIFR
	sbicrj PINE, 6, .LXB_Write_DoIO_Nothing
.LXB_doWriteTransaction_Primary:
	computeTransactionWindow
	getLowDataByte960                  							; Load lower byte from
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   							; Yes, so store to the EBI
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	getHighDataByte960                 							; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
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
	getHighDataByte960                 ; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	StoreHighByteIfBE1Low 1
	FallthroughExecutionBody_WriteOperation

.LXB_ShiftFromReadToWrite:
; start setting up for a write operation here
	setDataLinesDirection __zero_reg__ 	
	clearEIFR
	sbicrj PINE, 6, .LXB_Write_DoIO_Nothing
	computeTransactionWindow
	getLowDataByte960                  ; Load lower byte from
	SkipNextIfBE0High
	st Y, __low_data_byte960__		   ; Yes, so store to the EBI
	WhenBlastIsLowGoto	.LXB_do16BitWriteOperation
	getHighDataByte960                 ; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	signalReady 											
	std Y+1,__high_data_byte960__												; Store the upper byte to the EBI
	delay2cycles
	WhenBlastIsHighGoto .L642 ; this is checking blast for the second set of 16-bits not the first
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getDataWord960
	StoreHighByteIfBE1Low 3
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	FallthroughExecutionBody_WriteOperation
