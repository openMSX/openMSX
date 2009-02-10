.686
.XMM
.model flat, c

.code

; ResampleHQ_calcOutput_1_SSE
;   &buffer[bufIdx + filterLen16]
;   &table[tabIdx + filterLen16]
;   int* output
;   -4 * filterLen16
;   filterLenRest
; Preserved registers: edi, esi, ebp, ebx
ResampleHQ_calcOutput_1_SSE PROC C uses edi esi ebx bufferOffset:NEAR PTR, tableOffset:NEAR PTR, output:NEAR PTR, filterLen16Product:SDWORD, filterLenRest:DWORD

; ebx = bufferOffset
; ecx = tableOffset
; edi = output
; eax = filterLen16Product
; esi = filterLenRest
    mov         ebx,bufferOffset
    mov         ecx,tableOffset
    mov         edi,output
    mov         eax,filterLen16Product
    mov         esi,filterLenRest

    xorps       xmm0,xmm0
    xorps       xmm1,xmm1
    xorps       xmm2,xmm2
    xorps       xmm3,xmm3
label1:
    movups      xmm4,xmmword ptr [ebx+eax]
    mulps       xmm4,xmmword ptr [ecx+eax]
    movups      xmm5,xmmword ptr [ebx+eax+10h]
    mulps       xmm5,xmmword ptr [ecx+eax+10h]
    movups      xmm6,xmmword ptr [ebx+eax+20h]
    mulps       xmm6,xmmword ptr [ecx+eax+20h]
    movups      xmm7,xmmword ptr [ebx+eax+30h]
    mulps       xmm7,xmmword ptr [ecx+eax+30h]
    addps       xmm0,xmm4
    addps       xmm1,xmm5
    addps       xmm2,xmm6
    addps       xmm3,xmm7
    add         eax,40h
    jne         label1
    test        esi,8
    je          label2
    movups      xmm4,xmmword ptr [ebx+eax]
    mulps       xmm4,xmmword ptr [ecx+eax]
    movups      xmm5,xmmword ptr [ebx+eax+10h]
    mulps       xmm5,xmmword ptr [ecx+eax+10h]
    addps       xmm0,xmm4
    addps       xmm1,xmm5
    add         eax,20h
label2:
    test        esi,4
    je          label3
    movups      xmm6,xmmword ptr [ebx+eax]
    mulps       xmm6,xmmword ptr [ecx+eax]
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
    mov         dword ptr [edi],edx
    ret
ResampleHQ_calcOutput_1_SSE ENDP

end