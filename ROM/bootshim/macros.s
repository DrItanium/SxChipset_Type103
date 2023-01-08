/*
hitagimon
Copyright (c) 2020-2022, Joshua Scoggins
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
.macro clear_g14
        ldconst 0, g14 # c compiler expects g14 to be 0
.endm
.macro save_globals
/* -- We allocate a spot for a "register holder" on the stack
 *    and store data to that spot. We will take advantage of
 *    the fact that this will be allocated at the first spot on the stack
 */
        ldconst 64, r4
        addo    sp, r4, sp
        stq     g0, -64(sp)
        stq     g4, -48(sp)
        stq     g8, -32(sp)
        stt     g12, -16(sp)
 .endm
 .macro restore_globals
        ldq     -64(sp), g0
        ldq     -48(sp), g4
        ldq     -32(sp), g8
        ldt     -16(sp), g12
 .endm

.macro c_call function
    clear_g14
    call \function
.endm

.macro c_callx function
    clear_g14
    callx \function
.endm

.macro def_system_call index,name
.text
.align 4
.global _\()\name
_\()\name:
lda \index, g13
calls g13
ret
.endm