INCLUDE macamd64.inc

.code

; ResampleHQ_calcOutput_1_SSE
;   rcx = float* &buffer[bufIdx + filterLen16]
;   rdx = float* &table[tabIdx + filterLen16]
;   r8 = int* output
;   r9 = long -4 * filterLen16
;   [rsp+28h] = unsigned int filterLenRest
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY ResampleHQ_calcOutput_1_SSE, Video

; rcx = bufferOffset
; rdx = tableOffset
; r8 = output
; r9 = filterLen16Product
; r10 = filterLenRest
    movsxd      r9,r9d
    mov         r10d,dword ptr [rsp+28h]

    xorps       xmm0,xmm0
    xorps       xmm1,xmm1
    xorps       xmm2,xmm2
    xorps       xmm3,xmm3
label1:
    movups      xmm4,xmmword ptr [rcx+r9]
    mulps       xmm4,xmmword ptr [rdx+r9]
    movups      xmm5,xmmword ptr [rcx+r9+10h]
    mulps       xmm5,xmmword ptr [rdx+r9+10h]
    movups      xmm6,xmmword ptr [rcx+r9+20h]
    mulps       xmm6,xmmword ptr [rdx+r9+20h]
    movups      xmm7,xmmword ptr [rcx+r9+30h]
    mulps       xmm7,xmmword ptr [rdx+r9+30h]
    addps       xmm0,xmm4
    addps       xmm1,xmm5
    addps       xmm2,xmm6
    addps       xmm3,xmm7
    add         r9,40h
    jne         label1
    test        r10,8
    je          label2
    movups      xmm4,xmmword ptr [rcx+r9]
    mulps       xmm4,xmmword ptr [rdx+r9]
    movups      xmm5,xmmword ptr [rcx+r9+10h]
    mulps       xmm5,xmmword ptr [rdx+r9+10h]
    addps       xmm0,xmm4
    addps       xmm1,xmm5
    add         r9,20h
label2:
    test        r10,4
    je          label3
    movups      xmm6,xmmword ptr [rcx+r9]
    mulps       xmm6,xmmword ptr [rdx+r9]
    addps       xmm2,xmm6
label3:
    addps       xmm0,xmm1
    addps       xmm2,xmm3
    addps       xmm0,xmm2
    movaps      xmm7,xmm0
    shufps      xmm7,xmm0,4Eh
    addps       xmm7,xmm0
    movaps      xmm0,xmm7
    shufps      xmm0,xmm7,0B1h
    addss       xmm0,xmm7
    cvtss2si    edx,xmm0
    mov         dword ptr [r8],edx
    ret
LEAF_END ResampleHQ_calcOutput_1_SSE, Video

end