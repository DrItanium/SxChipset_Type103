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
PORTF = 0x11
DDRF = 0x10
PINF = 0x0F
PINK = 262
DDRK = 263
PORTK = 264
EIFR = 0x1c
PING = 0x12
AddressLinesInterface = 0x2300
MemoryWindowUpper = 0x22
IOSpaceHighest = 0xFE
TCNT2 = 0xb2
__rdy_signal_count_reg__ = 5
__eifr_mask_reg__ = 4
__direction_ff_reg__ = 3
__iospace_sec_reg__ = 2
.macro signalReady 
	sts TCNT2, __rdy_signal_count_reg__
.endm
.macro sbisrj a, b, dest
	sbis \a, \b
	rjmp \dest
.endm
.macro sbicrj a, b, dest
	sbic \a, \b
	rjmp \dest
.endm
.macro computeTransactionWindow
	lds r28, AddressLinesInterface
; load into Z
;	lds r30, AddressLinesInterface
;	ldi r31, MemoryWindowUpper
.endm
.macro setupRegisterConstants
; use the lowest registers to make sure that we have Y, r16, and r17 free for usage
	ldi r16, lo8(112)
	mov __eifr_mask_reg__, r16
	ldi r16, lo8(-3)
	mov __rdy_signal_count_reg__, r16 ; load the ready signal amount
	ldi r16, IOSpaceHighest
	mov __iospace_sec_reg__, r16
	ser r16
	mov __direction_ff_reg__, r16 ; 0xff
; setup index register Y to be the actual memory window, that way we can only update the lower half
	clr r28
	ldi r29, MemoryWindowUpper
.endm
.macro delay2cycles 
	rjmp 1f
1:
.endm
.macro delay6cycles
	lpm ; tmp_reg is used implicity, who cares what we get back
	lpm ; tmp_reg is used implicity, who cares what we get back
.endm

.macro clearEIFR
	out EIFR, __eifr_mask_reg__
.endm

.macro cpz reg
	cpi \reg, 0x00
.endm
.macro cpiospace reg
	cp \reg, __iospace_sec_reg__
.endm
.macro StoreToDataPort lo,hi
	out PORTF, \lo
	sts PORTK, \hi
.endm
.macro WhenBlastIsLowGoto dest
	sbisrj PING, 5, \dest
.endm
.macro WhenBlastIsHighGoto dest
	sbicrj PING, 5, \dest
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
.LXB_readOperation_DoNothing:
	out PORTF, __zero_reg__
	sts PORTK, __zero_reg__
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart ; if BLAST is low then we are done and just return
1:
	signalReady
	delay6cycles
	WhenBlastIsHighGoto 1b
.LXB_FirstSignalReady_ThenReadTransactionStart:
	signalReady
.LXB_ReadTransactionStart:
	sbisrj EIFR,4, .LXB_ReadTransactionStart ; keep waiting
	sbicrj EIFR,5, .LXB_ShiftFromReadToWrite; 
.LXB_PrimaryReadTransaction:
	clearEIFR						; Waiting for next memory transaction
	lds r24,AddressLinesInterface+3 ; Get the upper most byte to determine where to go
	cpz r24							; Zero?
	breq .LXB_ReadStreamingOperation     ; If so then start the read streaming operation
	cpiospace r24						 ; Nope, so check to see if it is the IO space
	brne .LXB_readOperation_DoNothing	 ; If it is not, then we do nothing
	call doIOReadOperation			     ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart		 ; And we are done :)
.LXB_ReadStreamingOperation: 
	computeTransactionWindow
	ld r25,Y
	ldd r24,Y+1
	StoreToDataPort r25,r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+2
	ldd r24,Y+3
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+4
	ldd r24,Y+5
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+6
	ldd r24,Y+7
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+8
	ldd r24,Y+9
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+10
	ldd r24,Y+11
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+12
	ldd r24,Y+13
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+14
	ldd r24,Y+15
	StoreToDataPort r25, r24
	rjmp .LXB_FirstSignalReady_ThenReadTransactionStart
.LXB_ShiftFromReadToWrite:
; start setting up for a write operation here
	out DDRF,__zero_reg__
	sts DDRK,__zero_reg__
	clearEIFR
; we need to use cpse instead of breq to allow for better jumping destination
	lds r24,AddressLinesInterface+3
	cpz r24                              ; are we looking at zero? If not then check 0xFE later on (1 cycle)
	breq .LXB_doWriteTransaction_Primary ; yes, it is a zero so jump (1 or 2 cycles)
	cpiospace r24 					     ; is this equal to 0xFE?
	brne 1f							     ; If they aren't equal then jump over and goto the do nothing action
	call doIOWriteOperation 			 ; perform the write operation
	rjmp .LXB_WriteTransactionStart		 ; restart execution
1:
	WhenBlastIsLowGoto .LXB_SignalReady_ThenWriteTransactionStart ; if BLAST is low then we are done and just return
1:
	signalReady
	delay6cycles
	WhenBlastIsHighGoto 1b
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_do16BitWriteOperation:
	sbis PING, 4 	   ; Is BE1 LOW?
	std Y+1, r25	   ; Yes, so store to the EBI
	rjmp .LXB_SignalReady_ThenWriteTransactionStart			 ; And we are done
.LXB_doWriteTransaction_Primary:
	computeTransactionWindow
	in r24,PINF 											; Load lower byte from F
	sbis PING, 3 	   ; Is BE0 LOW?
	st Y, r24		   ; Yes, so store to the EBI
	lds r25,PINK											; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	signalReady 											
	std Y+1,r25												; Store the upper byte to the EBI
	delay2cycles											; it takes 6 cycles (AVR) to trigger the ready signal, the std to the EBI with a one cycle delay takes four cycles (AVR) so we need to wait two more cycles to align everything
	WhenBlastIsHighGoto .L642 ; this is checking blast for the second set of 16-bits not the first
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	in r24,PINF			  ; load the lower byte
	lds r25, PINK		  ; Loading the port doesn't take much time so just do it regardless
	sbis PING, 4		  ; if BE1 is set then skip over the store
	std Y+3,r25			  ; Store to memory if applicable (this is the expensive part)
	std Y+2,r24			  ; save it without checking BE0 since we flowed into this part of the transaction
.LXB_SignalReady_ThenWriteTransactionStart:
	signalReady 
.LXB_WriteTransactionStart:
	sbisrj EIFR,4, .LXB_WriteTransactionStart
	sbisrj EIFR,5, .LXB_ShiftFromWriteToRead 
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpz r24
	breq .LXB_doWriteTransaction_Primary
	cpiospace r24
	brne 1f
	call doIOWriteOperation 
	rjmp .LXB_WriteTransactionStart
1:
	WhenBlastIsLowGoto .LXB_SignalReady_ThenWriteTransactionStart ; if BLAST is low then we are done and just return
1:
	signalReady
	delay6cycles
	WhenBlastIsHighGoto 1b
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L642:
	in r25,PINF
	lds r24,PINK
	signalReady 
	std Y+2,r25
	std Y+3,r24
	WhenBlastIsLowGoto .LXB_WriteBytes4_and_5_End
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+4,r25
	std Y+5,r24
	WhenBlastIsHighGoto .L649
	in r24,PINF
	lds r25, PINK
	sbis PING, 4
	std Y+7,r25
	std Y+6,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L649:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+6,r25
	std Y+7,r24
	WhenBlastIsLowGoto .LXB_WriteBytes8_and_9_End
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+8,r25
	std Y+9,r24
	WhenBlastIsHighGoto .L657
	in r24,PINF
	lds r25,PINK
	sbis PING, 4
	std Y+11,r25
	std Y+10,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L657:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+10,r25
	std Y+11,r24
	WhenBlastIsHighGoto .L661
	in r24,PINF
	lds r25,PINK
	sbis PING,4
	std Y+13,r25
	std Y+12,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L661:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+12,r25
	std Y+13,r24
	in r24,PINF
	lds r25, PINK
	sbis PING, 4
	std Y+15,r25
	std Y+14,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_WriteBytes4_and_5_End:
	in r24,PINF
	lds r25,PINK
	sbis PING, 4
	std Y+5,r25
	std Y+4,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_WriteBytes8_and_9_End:
	in r24,PINF
	lds r25,PINK
	sbis PING, 4
	std Y+9,r25
	std Y+8,r24
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_ShiftFromWriteToRead:
	out DDRF,__direction_ff_reg__	      ; Change the direction to output
	sts DDRK,__direction_ff_reg__         ; Change the direction to output
	clearEIFR						      ; Waiting for next memory transaction
	lds r24,AddressLinesInterface+3       ; Get the upper most byte to determine where to go
	cpz r24							      ; Zero?
	breq .LXB_ReadStreamingOperation2     ; If so then start the read streaming operation
	cpiospace r24 						  ; Nope, so check to see if it is the IO space
	brne 1f								  ; If it is not, then we do nothing
	call doIOReadOperation				  ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart		; And we are done :)
1:
	rjmp .LXB_readOperation_DoNothing
.LXB_ReadStreamingOperation2: 
	computeTransactionWindow
	ld r25,Y
	ldd r24,Y+1
	StoreToDataPort r25,r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+2
	ldd r24,Y+3
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+4
	ldd r24,Y+5
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+6
	ldd r24,Y+7
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+8
	ldd r24,Y+9
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+10
	ldd r24,Y+11
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+12
	ldd r24,Y+13
	StoreToDataPort r25, r24
	WhenBlastIsLowGoto .LXB_FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+14
	ldd r24,Y+15
	StoreToDataPort r25, r24
	rjmp .LXB_FirstSignalReady_ThenReadTransactionStart
