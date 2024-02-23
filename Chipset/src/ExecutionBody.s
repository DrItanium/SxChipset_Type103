.file "ExecutionBody.s"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__RAMPZ__ = 0x3b
__tmp_reg__ = 0
__zero_reg__ = 1
DDRF = 0x10
DDRK = 263
EIFR = 0x1c
PING = 0x12
AddressLinesInterface = 0x8000
MemoryWindowUpper = 0xFD
TCNT2 = 0xb2
__rdy_signal_count_reg__ = 28
__eifr_mask_reg__ = 29
__direction_ff_reg__ = 17
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
	lds r30, AddressLinesInterface
	ldi r31, MemoryWindowUpper
.endm
.global ExecutionBodyWithMemoryConnection
.global ExecutionBodyWithoutMemoryConnection
.global doIOReadOperation
.global doIOWriteOperation
.global doExternalCommunicationReadOperation
.global doExternalCommunicationWriteOperation
ExecutionBodyWithoutMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
	ldi __eifr_mask_reg__,lo8(112)
	ldi __rdy_signal_count_reg__,lo8(-3) ; load the ready signal amount
	ldi __direction_ff_reg__,lo8(-1) ; direction
	rjmp ReadTransactionStart ; jump into the top of the invocation loop
FirstSignalReady_ThenReadTransactionStart:
	signalReady
ReadTransactionStart:
	sbisrj EIFR,4, ReadTransactionStart
	sbisrj EIFR,5, .L564
	out DDRF,__zero_reg__
	sts DDRK,__zero_reg__
	out EIFR,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L634 ; equals zero
	cpi r24,lo8(-2) ; 0xFE
	brne .+2
	rjmp .L635
	sbisrj PING,5, SignalReady_ThenWriteTransactionStart
doNothingLoop0:
	signalReady 
	nop
	nop
	nop
	nop
	nop
	nop
	sbicrj PING,5, doNothingLoop0
	rjmp SignalReady_ThenWriteTransactionStart
.L634:
/* #APP */
 ;  871 "src/main.cc" 1
 computeTransactionWindow
	
 ;  0 "" 2
/* #NOAPP */
	sbisrj PING,5, .L636
	sbicrj PING,3, .L637
	in r24,0xf
	st Z,r24
.L637:
	lds r24,262
	signalReady 
	std Z+1,r24
	nop
	nop
	sbicrj PING, 5, .L642
	in r24,0xf
	std Z+2,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+3,r24
SignalReady_ThenWriteTransactionStart:
	signalReady 
.L618:
	sbisrj EIFR,4, .L618
	sbisrj EIFR,5, .L749
	out EIFR,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L634
	cpi r24,lo8(-2)
	breq .L635
	sbis PING,5
	rjmp SignalReady_ThenWriteTransactionStart
.L669:
	signalReady 
	nop
	nop
	nop
	nop
	nop
	nop
	sbicrj PING,5, .L669
	rjmp SignalReady_ThenWriteTransactionStart
.L635:
	call doIOWriteOperation 
	rjmp .L618
.L636:
	sbicrj PING,3, .L571
	in r24,0xf
	st Z,r24
.L571:
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+1,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L749:
	out 0x10,__direction_ff_reg__
	sts 263,__direction_ff_reg__
	out 0x1c,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L621
	cpi r24,lo8(-2)
	breq .L605
	out 0x11,__zero_reg__
	sts 264,__zero_reg__
	sbisrj PING, 5, .L728
.L632:
	signalReady 
	nop
	nop
	nop
	nop
	nop
	nop
	sbic PING,5
	rjmp .L632
.L728:
	rjmp FirstSignalReady_ThenReadTransactionStart
.L642:
	in r25,0xf
	lds r24,262
	signalReady 
	std Z+2,r25
	std Z+3,r24
	sbic PING,5
	rjmp .L645
	in r24,0xf
	std Z+4,r24
	sbic PING,4
	rjmp SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+5,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L605:
	call doIOReadOperation
	rjmp ReadTransactionStart
.L621:
/* #APP */
 ;  715 "src/main.cc" 1
 computeTransactionWindow
	
 ;  0 "" 2
/* #NOAPP */
	ld r25,Z
	ldd r24,Z+1
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady 
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
	rjmp FirstSignalReady_ThenReadTransactionStart
.L564:
	out 0x1c,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	brne .+2
	rjmp .L621
	cpi r24,lo8(-2)
	brne .+2
	rjmp .L605
	out 0x11,__zero_reg__
	sts 264,__zero_reg__
	sbisrj PING,5, .L728
.L617:
	signalReady 
	nop
	nop
	nop
	nop
	nop
	nop
	sbicrj PING,5, .L617
	rjmp FirstSignalReady_ThenReadTransactionStart
.L645:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+4,r25
	std Z+5,r24
	sbicrj PING,5, .L649
	in r24,0xf
	std Z+6,r24
	sbicrj PING,4,SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+7,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L649:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+6,r25
	std Z+7,r24
	sbicrj PING,5, .L653
	in r24,0xf
	std Z+8,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+9,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L653:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+8,r25
	std Z+9,r24
	sbicrj PING,5, .L657
	in r24,0xf
	std Z+10,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+11,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L657:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+10,r25
	std Z+11,r24
	sbicrj PING,5, .L661
	in r24,0xf
	std Z+12,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+13,r24
	rjmp SignalReady_ThenWriteTransactionStart
.L661:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+12,r25
	std Z+13,r24
	in r24,0xf
	std Z+14,r24
	sbicrj PING,4, SignalReady_ThenWriteTransactionStart
	lds r24,262
	std Z+15,r24
	rjmp SignalReady_ThenWriteTransactionStart
ExecutionBodyWithMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
	ldi __eifr_mask_reg__,lo8(112)
	ldi __rdy_signal_count_reg__,lo8(-3)
	ldi __direction_ff_reg__,lo8(-1)
.L829:
	sbisrj EIFR,4, .L829
	sbisrj EIFR,5, .L987
	out 0x10,__zero_reg__
	sts 263,__zero_reg__
	out 0x1c,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L893
.L970:
	cpi r24,lo8(-2)
	brne .L992
	call doIOWriteOperation 
.L880:
	sbisrj 0x1c,4, .L880
	sbisrj 0x1c,5, .L993
	out 0x1c,__eifr_mask_reg__
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
	in r24,0xf
	st Z,r24
.L896:
	lds r24,262
	signalReady
	std Z+1,r24
	nop
	nop
	sbicrj PING,5, .L901
	in r24,0xf
	std Z+2,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+3,r24
.L924:
	signalReady
	rjmp .L880
.L992:
	call doExternalCommunicationWriteOperation
	rjmp .L880
.L993:
	out 0x10,__direction_ff_reg__
	sts 263,__direction_ff_reg__
.L987:
	out 0x1c,__eifr_mask_reg__
	lds r24,AddressLinesInterface+3
	tst r24
	brne .+2
	rjmp .L882
	cpi r24,lo8(-2)
	breq .+2
	rjmp .L994
	call doIOReadOperation 
	rjmp .L829
.L895:
	sbicrj PING,3, .L898
	in r24,0xf
	st Z,r24
.L898:
	sbicrj PING,4, .L924
	lds r24,262
	std Z+1,r24
	rjmp .L924
.L901:
	in r25,0xf
	lds r24,262
	signalReady
	std Z+2,r25
	std Z+3,r24
	sbisrj PING,5, .L995
	in r25,0xf
	lds r24,262
	signalReady
	std Z+4,r25
	std Z+5,r24
	sbisrj PING,5, .L996
	in r25,0xf
	lds r24,262
	signalReady
	std Z+6,r25
	std Z+7,r24
	sbis PING,5
	rjmp .L997
	in r25,0xf
	lds r24,262
	signalReady
	std Z+8,r25
	std Z+9,r24
	sbis PING,5
	rjmp .L998
	in r25,0xf
	lds r24,262
	signalReady
	std Z+10,r25
	std Z+11,r24
	sbis PING,5
	rjmp .L999
	in r25,0xf
	lds r24,262
	signalReady
	std Z+12,r25
	std Z+13,r24
	in r24,0xf
	std Z+14,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+15,r24
	rjmp .L924
.L994:
	call doExternalCommunicationReadOperation
	rjmp .L829
.L882:
/* #APP */
 ;  715 "src/main.cc" 1
computeTransactionWindow
	
 ;  0 "" 2
/* #NOAPP */
	ld r25,Z
	ldd r24,Z+1
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbisrj PING,5, .L967
	signalReady
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbisrj PING, 5, .L967
	signalReady
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
.L967:
	signalReady
	rjmp .L829
.L995:
	in r24,0xf
	std Z+4,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+5,r24
	rjmp .L924
.L996:
	in r24,0xf
	std Z+6,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+7,r24
	rjmp .L924
.L997:
	in r24,0xf
	std Z+8,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+9,r24
	rjmp .L924
.L998:
	in r24,0xf
	std Z+10,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+11,r24
	rjmp .L924
.L999:
	in r24,0xf
	std Z+12,r24
	sbicrj PING,4, .L924
	lds r24,262
	std Z+13,r24
	rjmp .L924
