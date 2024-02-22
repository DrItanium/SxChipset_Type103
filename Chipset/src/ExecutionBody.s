.file "ExecutionBody.s"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__RAMPZ__ = 0x3b
__tmp_reg__ = 0
__zero_reg__ = 1
AddressLinesInterface=0x8000
MemoryWindowUpper=0xFD
.global _ExecutionBodyWithMemoryConnection
.global _ExecutionBodyWithoutMemoryConnection
_ExecutionBodyWithoutMemoryConnection:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
.L__stack_usage = 0
	ldi r29,lo8(112)
	ldi r28,lo8(-3)
	ldi r17,lo8(-1)
.L563:
	sbis 0x1c,4
	rjmp .L563
	sbis 0x1c,5
	rjmp .L564
	out 0x10,__zero_reg__
	sts 263,__zero_reg__
	out 0x1c,r29
	lds r24,AddressLinesInterface+3
	tst r24
	breq .L634
	cpi r24,lo8(-2)
	brne .+2
	rjmp .L635
	sbis 0x12,5
	rjmp .L668
.L602:
	sts 178,r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic 0x12,5
	rjmp .L602
	rjmp .L668
.L634:
/* #APP */
 ;  871 "src/main.cc" 1
	lds r30, AddressLinesInterface
	ldi r31, MemoryWindowUpper
	
 ;  0 "" 2
/* #NOAPP */
	sbis 0x12,5
	rjmp .L636
	sbic 0x12,3
	rjmp .L637
	in r24,0xf
	st Z,r24
.L637:
	lds r24,262
	sts 178,r28
	std Z+1,r24
	nop
	nop
	sbic 0x12,5
	rjmp .L642
	in r24,0xf
	std Z+2,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+3,r24
.L668:
	sts 178,r28
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
	sbis 0x12,5
	rjmp .L668
.L669:
	sts 178,r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic 0x12,5
	rjmp .L669
	rjmp .L668
.L635:
	call _Z4doIOILb0EEvv
	rjmp .L618
.L636:
	sbic 0x12,3
	rjmp .L571
	in r24,0xf
	st Z,r24
.L571:
	sbic 0x12,4
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
	sbis 0x12,5
	rjmp .L728
.L632:
	sts 178,r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic 0x12,5
	rjmp .L632
.L728:
	sts 178,r28
	rjmp .L563
.L642:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+2,r25
	std Z+3,r24
	sbic 0x12,5
	rjmp .L645
	in r24,0xf
	std Z+4,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+5,r24
	rjmp .L668
.L605:
	call _Z4doIOILb1EEvv
	rjmp .L563
.L621:
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
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L728
	sts 178,r28
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
	sts 178,r28
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
	sbis 0x12,5
	rjmp .L728
.L617:
	sts 178,r28
	nop
	nop
	nop
	nop
	nop
	nop
	sbic 0x12,5
	rjmp .L617
	sts 178,r28
	rjmp .L563
.L645:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+4,r25
	std Z+5,r24
	sbic 0x12,5
	rjmp .L649
	in r24,0xf
	std Z+6,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+7,r24
	rjmp .L668
.L649:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+6,r25
	std Z+7,r24
	sbic 0x12,5
	rjmp .L653
	in r24,0xf
	std Z+8,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+9,r24
	rjmp .L668
.L653:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+8,r25
	std Z+9,r24
	sbic 0x12,5
	rjmp .L657
	in r24,0xf
	std Z+10,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+11,r24
	rjmp .L668
.L657:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+10,r25
	std Z+11,r24
	sbic 0x12,5
	rjmp .L661
	in r24,0xf
	std Z+12,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+13,r24
	rjmp .L668
.L661:
	in r25,0xf
	lds r24,262
	sts 178,r28
	std Z+12,r25
	std Z+13,r24
	in r24,0xf
	std Z+14,r24
	sbic 0x12,4
	rjmp .L668
	lds r24,262
	std Z+15,r24
	rjmp .L668
_ExecutionBodyWithMemoryConnection:
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
	call _Z4doIOILb0EEvv
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
	sbis 0x12,5
	rjmp .L895
	sbic 0x12,3
	rjmp .L896
	in r24,0xf
	st Z,r24
.L896:
	lds r24,262
	sts 178,r29
	std Z+1,r24
	nop
	nop
	sbic 0x12,5
	rjmp .L901
	in r24,0xf
	std Z+2,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+3,r24
.L924:
	sts 178,r29
	rjmp .L880
.L992:
	call _Z23doExternalCommunicationILb0EEvv
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
	call _Z4doIOILb1EEvv
	rjmp .L829
.L895:
	sbic 0x12,3
	rjmp .L898
	in r24,0xf
	st Z,r24
.L898:
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+1,r24
	rjmp .L924
.L901:
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+2,r25
	std Z+3,r24
	sbis 0x12,5
	rjmp .L995
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+4,r25
	std Z+5,r24
	sbis 0x12,5
	rjmp .L996
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+6,r25
	std Z+7,r24
	sbis 0x12,5
	rjmp .L997
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+8,r25
	std Z+9,r24
	sbis 0x12,5
	rjmp .L998
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+10,r25
	std Z+11,r24
	sbis 0x12,5
	rjmp .L999
	in r25,0xf
	lds r24,262
	sts 178,r29
	std Z+12,r25
	std Z+13,r24
	in r24,0xf
	std Z+14,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+15,r24
	rjmp .L924
.L994:
	call _Z23doExternalCommunicationILb1EEvv
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
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+2
	ldd r24,Z+3
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+4
	ldd r24,Z+5
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+6
	ldd r24,Z+7
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+8
	ldd r24,Z+9
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+10
	ldd r24,Z+11
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+12
	ldd r24,Z+13
	out 0x11,r25
	sts 264,r24
	sbis 0x12,5
	rjmp .L967
	sts 178,r29
	ldd r25,Z+14
	ldd r24,Z+15
	out 0x11,r25
	sts 264,r24
.L967:
	sts 178,r29
	rjmp .L829
.L995:
	in r24,0xf
	std Z+4,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+5,r24
	rjmp .L924
.L996:
	in r24,0xf
	std Z+6,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+7,r24
	rjmp .L924
.L997:
	in r24,0xf
	std Z+8,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+9,r24
	rjmp .L924
.L998:
	in r24,0xf
	std Z+10,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+11,r24
	rjmp .L924
.L999:
	in r24,0xf
	std Z+12,r24
	sbic 0x12,4
	rjmp .L924
	lds r24,262
	std Z+13,r24
	rjmp .L924
