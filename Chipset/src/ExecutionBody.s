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
.macro signalReady name
sts TCNT2, \name 
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
.L__stack_usage = 0
	ldi r29,lo8(112)
	ldi r28,lo8(-3) ; load the ready signal amount
	ldi r17,lo8(-1) ; direction
.L563:
	sbis EIFR,4
	rjmp .L563
	sbis EIFR,5
	rjmp .L564
	out DDRF,__zero_reg__
	sts DDRK,__zero_reg__
	out EIFR,r29
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L634
	cpi r24,lo8(-2)
	brne .+2
	rjmp .L635
	sbis PING,5
	rjmp .L668
.L602:
	signalReady r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic PING,5
	rjmp .L602
	rjmp .L668
.L634:
/* #APP */
 ;  871 "src/main.cc" 1
	lds r30, AddressLinesInterface
	ldi r31, MemoryWindowUpper
	
 ;  0 "" 2
/* #NOAPP */
	sbis PING,5
	rjmp .L636
	sbic PING,3
	rjmp .L637
	in r24,0xf
	st Z,r24
.L637:
	lds r24,262
	signalReady r28
	std Z+1,r24
	nop
	nop
	sbic PING,5
	rjmp .L642
	in r24,0xf
	std Z+2,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+3,r24
.L668:
	signalReady r28
.L618:
	sbis 0x1c,4
	rjmp .L618
	sbis 0x1c,5
	rjmp .L749
	out 0x1c,r29
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L634
	cpi r24,lo8(-2)
	breq .L635
	sbis PING,5
	rjmp .L668
.L669:
	signalReady r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic PING,5
	rjmp .L669
	rjmp .L668
.L635:
	call doIOWriteOperation 
	rjmp .L618
.L636:
	sbic PING,3
	rjmp .L571
	in r24,0xf
	st Z,r24
.L571:
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+1,r24
	rjmp .L668
.L749:
	out 0x10,r17
	sts 263,r17
	out 0x1c,r29
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L621
	cpi r24,lo8(-2)
	breq .L605
	out 0x11,__zero_reg__
	sts 264,__zero_reg__
	sbis PING,5
	rjmp .L728
.L632:
	signalReady r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic PING,5
	rjmp .L632
.L728:
	signalReady r28
	rjmp .L563
.L642:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+2,r25
	std Z+3,r24
	sbic PING,5
	rjmp .L645
	in r24,0xf
	std Z+4,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+5,r24
	rjmp .L668
.L605:
	call doIOReadOperation
	rjmp .L563
.L621:
/* #APP */
 ;  715 "src/main.cc" 1
	lds r30, AddressLinesInterface
	ldi r31, MemoryWindowUpper
	
 ;  0 "" 2
/* #NOAPP */
	ld r25,Z
	ldd r24,Z+1
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L728
	signalReady r28
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
	signalReady r28
	rjmp .L563
.L564:
	out 0x1c,r29
	lds r24,AddressLinesInterface+3
	tst r24
	brne .+2
	rjmp .L621
	cpi r24,lo8(-2)
	brne .+2
	rjmp .L605
	out 0x11,__zero_reg__
	sts 264,__zero_reg__
	sbis PING,5
	rjmp .L728
.L617:
	signalReady r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic PING,5
	rjmp .L617
	signalReady r28
	rjmp .L563
.L645:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+4,r25
	std Z+5,r24
	sbic PING,5
	rjmp .L649
	in r24,0xf
	std Z+6,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+7,r24
	rjmp .L668
.L649:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+6,r25
	std Z+7,r24
	sbic PING,5
	rjmp .L653
	in r24,0xf
	std Z+8,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+9,r24
	rjmp .L668
.L653:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+8,r25
	std Z+9,r24
	sbic PING,5
	rjmp .L657
	in r24,0xf
	std Z+10,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+11,r24
	rjmp .L668
.L657:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+10,r25
	std Z+11,r24
	sbic PING,5
	rjmp .L661
	in r24,0xf
	std Z+12,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+13,r24
	rjmp .L668
.L661:
	in r25,0xf
	lds r24,262
	signalReady r28
	std Z+12,r25
	std Z+13,r24
	in r24,0xf
	std Z+14,r24
	sbic PING,4
	rjmp .L668
	lds r24,262
	std Z+15,r24
	rjmp .L668
ExecutionBodyWithMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
.L__stack_usage = 0
	ldi r28,lo8(112)
	ldi r29,lo8(-3)
	ldi r17,lo8(-1)
.L829:
	sbis 0x1c,4
	rjmp .L829
	sbis 0x1c,5
	rjmp .L987
	out 0x10,__zero_reg__
	sts 263,__zero_reg__
	out 0x1c,r28
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L893
.L970:
	cpi r24,lo8(-2)
	brne .L992
	call doIOWriteOperation 
.L880:
	sbis 0x1c,4
	rjmp .L880
	sbis 0x1c,5
	rjmp .L993
	out 0x1c,r28
	lds r24,AddressLinesInterface+3
	cpse r24,__zero_reg__
	rjmp .L970
.L893:
/* #APP */
 ;  871 "src/main.cc" 1
	lds r30, 0x8000
	ldi r31, 0xFD
	
 ;  0 "" 2
/* #NOAPP */
	sbis PING,5
	rjmp .L895
	sbic PING,3
	rjmp .L896
	in r24,0xf
	st Z,r24
.L896:
	lds r24,262
	signalReady r29
	std Z+1,r24
	nop
	nop
	sbic PING,5
	rjmp .L901
	in r24,0xf
	std Z+2,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+3,r24
.L924:
	signalReady r29
	rjmp .L880
.L992:
	call doExternalCommunicationWriteOperation
	rjmp .L880
.L993:
	out 0x10,r17
	sts 263,r17
.L987:
	out 0x1c,r28
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
	sbic PING,3
	rjmp .L898
	in r24,0xf
	st Z,r24
.L898:
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+1,r24
	rjmp .L924
.L901:
	in r25,0xf
	lds r24,262
	signalReady r29
	std Z+2,r25
	std Z+3,r24
	sbis PING,5
	rjmp .L995
	in r25,0xf
	lds r24,262
	signalReady r29
	std Z+4,r25
	std Z+5,r24
	sbis PING,5
	rjmp .L996
	in r25,0xf
	lds r24,262
	signalReady r29
	std Z+6,r25
	std Z+7,r24
	sbis PING,5
	rjmp .L997
	in r25,0xf
	lds r24,262
	signalReady r29
	std Z+8,r25
	std Z+9,r24
	sbis PING,5
	rjmp .L998
	in r25,0xf
	lds r24,262
	signalReady r29
	std Z+10,r25
	std Z+11,r24
	sbis PING,5
	rjmp .L999
	in r25,0xf
	lds r24,262
	signalReady r29
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
	lds r30, 0x8000
	ldi r31, 0xFD
	
 ;  0 "" 2
/* #NOAPP */
	ld r25,Z
	ldd r24,Z+1
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbis PING,5
	rjmp .L967
	signalReady r29
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
.L967:
	signalReady r29
	rjmp .L829
.L995:
	in r24,0xf
	std Z+4,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+5,r24
	rjmp .L924
.L996:
	in r24,0xf
	std Z+6,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+7,r24
	rjmp .L924
.L997:
	in r24,0xf
	std Z+8,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+9,r24
	rjmp .L924
.L998:
	in r24,0xf
	std Z+10,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+11,r24
	rjmp .L924
.L999:
	in r24,0xf
	std Z+12,r24
	sbic PING,4
	rjmp .L924
	lds r24,262
	std Z+13,r24
	rjmp .L924
