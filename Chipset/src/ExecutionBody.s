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
AddressLinesInterface = 0x8000
MemoryWindowUpper = 0xFD
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
	ldi r16, lo8(-2)
	mov __iospace_sec_reg__, r16
	ser r16
	mov __direction_ff_reg__, r16 ; 0xff
; setup index register Y to be the actual memory window, that way we can only update the lower half
	clr r28
	ldi r29, MemoryWindowUpper
.endm
.global ExecutionBodyWithoutMemoryConnection
.global ExecutionBodyWithMemoryConnection
.global doIOReadOperation
.global doIOWriteOperation
.global doExternalCommunicationReadOperation
.global doExternalCommunicationWriteOperation
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
.text

readOperation_DoNothing:
	out PORTF,__zero_reg__
	sts PORTK,__zero_reg__
writeOperation_DoNothing:
	sbisrj PING, 5, readOperation_DoNothing_Done ; if BLAST is low then we are done and just return
readOperation_DoNothing_LoopBody:
	signalReady
	delay6cycles
	sbicrj PING, 5, readOperation_DoNothing_LoopBody
readOperation_DoNothing_Done:
	signalReady
	ret
	
ExecutionBodyWithoutMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
	setupRegisterConstants
	rjmp ReadTransactionStart ; jump into the top of the invocation loop
FirstSignalReady_ThenReadTransactionStart:
	signalReady
ReadTransactionStart:
	sbisrj EIFR,4, ReadTransactionStart ; keep waiting
	sbisrj EIFR,5, PrimaryReadTransaction  ; 
; start setting up for a write operation here
	out DDRF,__zero_reg__
	sts DDRK,__zero_reg__
	clearEIFR
; we need to use cpse instead of breq to allow for better jumping destination
	lds r24,AddressLinesInterface+3
	cpz r24  ; are we looking at zero? If not then check 0xFE later on (1 cycle)
	breq doWriteTransaction_Primary ; yes, it is a zero so jump (1 or 2 cycles)
									; total is 2 cycles when it isn't true and three cycles when it is
									; must keep the operation local though...
	cp r24, __iospace_sec_reg__   ; is this equal to 0xFE
	brne 1f						  ; If they aren't equal then jump over and goto the do nothing action
	call doIOWriteOperation
	rjmp WriteTransactionStart
1:
	call writeOperation_DoNothing
	rjmp WriteTransactionStart
doWriteTransaction_Primary:
	computeTransactionWindow
	sbisrj PING,5, do16BitWriteOperation 				; Is blast high? then keep going, otherwise it is a 8/16-bit operations
	sbicrj PING,3, 1f 
; singular operation
	in r24,PINF
	st Y,r24
1:
	lds r24,PINK
	signalReady 
	std Y+1,r24
	delay2cycles
	sbicrj PING, 5, .L642
	; this is a 32-bit write operation so we want to check BE1 and then fallthrough to the execution body itself
	in r24,PINF
	std Y+2,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+3,r24
SignalReady_ThenWriteTransactionStart:
	signalReady 
WriteTransactionStart:
	sbisrj EIFR,4, WriteTransactionStart
	sbisrj EIFR,5, ShiftFromWriteToRead 
	clearEIFR
	lds r24,AddressLinesInterface+3
	tst r24
	breq doWriteTransaction_Primary
	cp r24, __iospace_sec_reg__
	brne 1f
	call doIOWriteOperation 
	rjmp WriteTransactionStart
1:
	call writeOperation_DoNothing
	rjmp WriteTransactionStart
do16BitWriteOperation:
	sbicrj PING,3, 1f 
	in r24,PINF
	st Y,r24
1:
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+1,r24
	rjmp SignalReady_ThenWriteTransactionStart
ShiftFromWriteToRead:
	out DDRF,__direction_ff_reg__
	sts 263,__direction_ff_reg__
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpz r24
	breq ReadStreamingOperation 
	cp r24, __iospace_sec_reg__
	brne 1f
	call doIOReadOperation
	rjmp ReadTransactionStart
1:
	call readOperation_DoNothing
	rjmp ReadTransactionStart
.L642:
	in r25,PINF
	lds r24,PINK
	signalReady 
	std Y+2,r25
	std Y+3,r24
	sbicrj PING,5, .L645
	in r24,PINF
	std Y+4,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+5,r24
	rjmp SignalReady_ThenWriteTransactionStart
ReadStreamingOperation: 
	computeTransactionWindow
	ld r25,Y
	ldd r24,Y+1
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+2
	ldd r24,Y+3
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+4
	ldd r24,Y+5
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+6
	ldd r24,Y+7
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+8
	ldd r24,Y+9
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+10
	ldd r24,Y+11
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+12
	ldd r24,Y+13
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, FirstSignalReady_ThenReadTransactionStart
	signalReady 
	ldd r25,Y+14
	ldd r24,Y+15
	out 0x11,r25
	sts PORTK,r24
	rjmp FirstSignalReady_ThenReadTransactionStart
PrimaryReadTransaction:
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpz r24
	brne 1f
	rjmp ReadStreamingOperation 
1:
	cp r24, __iospace_sec_reg__
	brne 1f
	call doIOReadOperation
	rjmp ReadTransactionStart
1:
	call readOperation_DoNothing
	rjmp ReadTransactionStart
.L645:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+4,r25
	std Y+5,r24
	sbicrj PING,5, .L649
	in r24,PINF
	std Y+6,r24
	sbicrj PING,4,SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+7,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L649:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+6,r25
	std Y+7,r24
	sbicrj PING,5, .L653
	in r24,PINF
	std Y+8,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+9,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L653:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+8,r25
	std Y+9,r24
	sbicrj PING,5, .L657
	in r24,PINF
	std Y+10,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+11,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L657:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+10,r25
	std Y+11,r24
	sbicrj PING,5, .L661
	in r24,PINF
	std Y+12,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+13,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L661:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+12,r25
	std Y+13,r24
	in r24,PINF
	std Y+14,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,PINK
	std Y+15,r24
	rjmp SignalReady_ThenWriteTransactionStart




ExecutionBodyWithMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
	setupRegisterConstants

.L829:
	sbisrj EIFR,4, .L829
	sbisrj EIFR,5, .L987
	out DDRF,__zero_reg__
	sts 263,__zero_reg__
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpz r24
	breq .L893
.L970:
	cp r24, __iospace_sec_reg__
	brne .L992
	call doIOWriteOperation 
.L880:
	sbisrj 0x1c,4, .L880
	sbisrj 0x1c,5, .L993
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpse r24,__zero_reg__
	rjmp .L970
.L893:
/* #APP */
 ;  871 "src/main.cc" 1
 computeTransactionWindow
	
 ;  0 "" 2
/* #NOAPP */
	sbisrj PING,5, .L895
	sbicrj PING,3, .L896
	in r24,PINF
	st Y,r24
.L896:
	lds r24,PINK
	signalReady
	std Y+1,r24
	delay2cycles
	sbicrj PING,5, .L901
	in r24,PINF
	std Y+2,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+3,r24
.L924:
	signalReady
	rjmp .L880
.L992:
	call doExternalCommunicationWriteOperation
	rjmp .L880
.L993:
	out DDRF,__direction_ff_reg__
	sts 263,__direction_ff_reg__
.L987:
	clearEIFR
	lds r24,AddressLinesInterface+3
	cpz r24
	brne 1f
	rjmp .L882
1:
	cp r24,__iospace_sec_reg__
	breq 1f
	call doExternalCommunicationReadOperation
	rjmp .L829
1:
	call doIOReadOperation 
	rjmp .L829
.L895:
	sbicrj PING,3, .L898
	in r24,PINF
	st Y,r24
.L898:
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+1,r24
	rjmp .L924
.L901:
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+2,r25
	std Y+3,r24
	sbisrj PING,5, .L995
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+4,r25
	std Y+5,r24
	sbisrj PING,5, .L996
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+6,r25
	std Y+7,r24
	sbisrj PING,5, .L997
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+8,r25
	std Y+9,r24
	sbisrj PING,5, .L998
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+10,r25
	std Y+11,r24
	sbisrj PING,5, .L999
	in r25,PINF
	lds r24,PINK
	signalReady
	std Y+12,r25
	std Y+13,r24
	in r24,PINF
	std Y+14,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+15,r24
	rjmp .L924
.L882:
/* #APP */
 ;  715 "src/main.cc" 1
computeTransactionWindow
	
 ;  0 "" 2
/* #NOAPP */
	ld r25,Y
	ldd r24,Y+1
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+2
	ldd r24,Y+3
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+4
	ldd r24,Y+5
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+6
	ldd r24,Y+7
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+8
	ldd r24,Y+9
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+10
	ldd r24,Y+11
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Y+12
	ldd r24,Y+13
	out 0x11,r25
	sts PORTK,r24
	sbisrj PING, 5, .L967
	signalReady
	ldd r25,Y+14
	ldd r24,Y+15
	out 0x11,r25
	sts PORTK,r24
.L967:
	signalReady
	rjmp .L829
.L995:
	in r24,PINF
	std Y+4,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+5,r24
	rjmp .L924
.L996:
	in r24,PINF
	std Y+6,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+7,r24
	rjmp .L924
.L997:
	in r24,PINF
	std Y+8,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+9,r24
	rjmp .L924
.L998:
	in r24,PINF
	std Y+10,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+11,r24
	rjmp .L924
.L999:
	in r24,PINF
	std Y+12,r24
	sbicrj PING,4, .L924
	lds r24,PINK
	std Y+13,r24
	rjmp .L924
