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
ExternalAddressLinesInterface = 0x2300
AddressLinesInterface = ExternalAddressLinesInterface
MemoryWindowUpper = 0x22
IOSpaceHighest = 0xFE
TCNT2 = 0xb2
__high_data_byte960__ = 7
__low_data_byte960__ = 6
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
	cp \reg, __zero_reg__
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
.macro getLowDataByte960 
	in __low_data_byte960__, PINF
.endm
.macro getHighDataByte960
	lds __high_data_byte960__, PINK
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
	sbicrj PINE, 2, .LXB_readOperation_DoNothing
	call doIOReadOperation			      ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart		  ; And we are done :)
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

.LXB_ShiftFromWriteToRead_CheckIO_Nothing:
	sbicrj PINE, 2, .LXB_readOperation_DoNothing
	call doIOReadOperation				  ; It is so call doIOReadOperation, back to c++
	rjmp .LXB_ReadTransactionStart		; And we are done :)
.LXB_ShiftFromWriteToRead:
	out DDRF,__direction_ff_reg__	      ; Change the direction to output
	sts DDRK,__direction_ff_reg__         ; Change the direction to output
	clearEIFR						      ; Waiting for next memory transaction
	sbicrj PINE, 6, .LXB_ShiftFromWriteToRead_CheckIO_Nothing
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
1:
	signalReady
	delay6cycles
	WhenBlastIsHighGoto 1b
.LXB_SignalReady_ThenWriteTransactionStart:
	signalReady 
.LXB_WriteTransactionStart:
	sbisrj EIFR,4, .LXB_WriteTransactionStart
	sbisrj EIFR,5, .LXB_ShiftFromWriteToRead 
	clearEIFR
	sbicrj PINE, 6, .LXB_Write_DoIO_Nothing
.LXB_doWriteTransaction_Primary:
	computeTransactionWindow
	WhenBlastIsLowGoto .LXB_do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	getLowDataByte960                  ; Load lower byte from
	sbis PING, 3 	                   ; Is BE0 LOW?
	st Y, __low_data_byte960__		   ; Yes, so store to the EBI
	getHighDataByte960                 ; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	signalReady 											
	std Y+1,__high_data_byte960__												; Store the upper byte to the EBI
	delay2cycles
	WhenBlastIsHighGoto .L642 ; this is checking blast for the second set of 16-bits not the first
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4		          ; if BE1 is set then skip over the store
	std Y+3,__high_data_byte960__ ; Store to memory if applicable (this is the expensive part)
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L642:
	getLowDataByte960
	getHighDataByte960
	signalReady 
	std Y+2,__low_data_byte960__
	std Y+3,__high_data_byte960__
	WhenBlastIsLowGoto .LXB_WriteBytes4_and_5_End
	getLowDataByte960
	getHighDataByte960
	signalReady
	std Y+4,__low_data_byte960__
	std Y+5,__high_data_byte960__
	WhenBlastIsHighGoto .L649
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4
	std Y+7,__high_data_byte960__
	std Y+6,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L649:
	getLowDataByte960
	getHighDataByte960
	signalReady
	std Y+6,__low_data_byte960__
	std Y+7,__high_data_byte960__
	WhenBlastIsLowGoto .LXB_WriteBytes8_and_9_End
	getLowDataByte960
	getHighDataByte960
	signalReady
	std Y+8,__low_data_byte960__
	std Y+9,__high_data_byte960__
	WhenBlastIsHighGoto .L657
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4
	std Y+11,__high_data_byte960__
	std Y+10,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L657:
	getLowDataByte960
	getHighDataByte960
	signalReady
	std Y+10,__low_data_byte960__
	std Y+11,__high_data_byte960__
	WhenBlastIsHighGoto .L661
	getLowDataByte960
	getHighDataByte960
	sbis PING,4
	std Y+13,__high_data_byte960__
	std Y+12,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.L661:
	getLowDataByte960
	getHighDataByte960
	signalReady
	std Y+12,__low_data_byte960__
	std Y+13,__high_data_byte960__
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4
	std Y+15,__high_data_byte960__
	std Y+14,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_WriteBytes4_and_5_End:
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4
	std Y+5,__high_data_byte960__
	std Y+4,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_WriteBytes8_and_9_End:
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4
	std Y+9,__high_data_byte960__
	std Y+8,__low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_do16BitWriteOperation:
	getHighDataByte960                 ; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	sbis PING, 4 	   ; Is BE1 LOW?
	std Y+1, __high_data_byte960__	   ; Yes, so store to the EBI
	getLowDataByte960
	sbis PING, 3
	st Y, __low_data_byte960__
	rjmp .LXB_SignalReady_ThenWriteTransactionStart			 ; And we are done

.LXB_ShiftFromReadToWrite_DoIO_Nothing:
	sbicrj PINE, 2, 1f
	call doIOWriteOperation 			 ; perform the write operation
	rjmp .LXB_WriteTransactionStart		 ; restart execution
1:
	WhenBlastIsLowGoto .LXB_SignalReady_ThenWriteTransactionStart ; if BLAST is low then we are done and just return
1:
	signalReady
	delay6cycles
	WhenBlastIsHighGoto 1b
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
.LXB_ShiftFromReadToWrite:
; start setting up for a write operation here
	out DDRF,__zero_reg__
	sts DDRK,__zero_reg__
	clearEIFR
	sbicrj PINE, 6, .LXB_ShiftFromReadToWrite_DoIO_Nothing
	computeTransactionWindow
	WhenBlastIsLowGoto	.LXB_do16BitWriteOperation
	getLowDataByte960                  ; Load lower byte from
	sbis PING, 3 	                   ; Is BE0 LOW?
	st Y, __low_data_byte960__		   ; Yes, so store to the EBI
	getHighDataByte960                 ; At this point we know that we will always be writing the upper byte (we are flowing to the next 16-bits)
	signalReady 											
	std Y+1,__high_data_byte960__												; Store the upper byte to the EBI
	delay2cycles
	WhenBlastIsHighGoto .L642 ; this is checking blast for the second set of 16-bits not the first
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	getLowDataByte960
	getHighDataByte960
	sbis PING, 4		          ; if BE1 is set then skip over the store
	std Y+3,__high_data_byte960__ ; Store to memory if applicable (this is the expensive part)
	std Y+2,__low_data_byte960__  ; save it without checking BE0 since we flowed into this part of the transaction
	rjmp .LXB_SignalReady_ThenWriteTransactionStart
