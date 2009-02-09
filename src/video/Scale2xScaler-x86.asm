.686
.XMM
.model flat, c

.code

; Scale2xScaler_scaleLineHalf_1on2_4_SSE
;   Pixel* dst
;   const Pixel* src0
;   const Pixel* src1
;   const Pixel* src2
;   unsigned long srcWidth
; Preserved registers: edi, esi, ebp, ebx
Scale2xScaler_scaleLineHalf_1on2_4_SSE PROC C uses esi ebx dst:NEAR PTR, src0:NEAR PTR, src1:NEAR PTR, src2:NEAR, srcWidth:DWORD

; esi = src1 + srcWidth - 2
; ebx = src0 + srcWidth - 2
; ecx = src2 + srcWidth - 2
; edx = dst + 2 * (srcWidth - 2)
; eax = -4 * (srcWidth - 2)

    mov         eax,srcWidth
    sub         eax,2
    shl         eax,2
    mov         esi,src1
    lea         esi,[esi+eax]
    mov         ebx,src0
    lea         ebx,[ebx+eax]
    mov         ecx,src2
    lea         ecx,[ecx+eax]
    mov         edx,dst
    lea         edx,[edx+2*eax]
    neg         eax

    movq        mm1,mmword ptr [esi+eax]
    pshufw      mm0,mm1,0EEh

mainloop:
    movq        mm2,mmword ptr [ebx+eax]
    movq        mm3,mm2
    pshufw      mm5,mm2,44h
    pcmpeqd     mm3,mmword ptr [ecx+eax]
    movq        mm7,mm0
    pshufw      mm4,mm3,44h
    punpckhdq   mm7,mm1
    pcmpeqd     mm7,mm5
    pandn       mm4,mm7
    pshufw      mm6,mm7,4Eh
    pandn       mm6,mm4
    pshufw      mm7,mm1,44h
    pand        mm5,mm6
    movq        mm0,mmword ptr [esi+eax+8]
    pandn       mm6,mm7
    por         mm5,mm6
    pshufw      mm7,mm2,0EEh
    movq        mm2,mm1
    pshufw      mm4,mm3,0EEh
    punpckldq   mm2,mm0
    pcmpeqd     mm2,mm7
    pandn       mm4,mm2
    pshufw      mm6,mm2,4Eh
    pandn       mm6,mm4
    pshufw      mm3,mm1,0EEh
    pand        mm7,mm6
    movq        mm2,mm0
    pandn       mm6,mm3
    movq        mm0,mm1
    por         mm7,mm6
    movq        mm1,mm2
    movntq      mmword ptr [edx+eax*2],mm5
    movntq      mmword ptr [edx+eax*2+8],mm7
    add         eax,8
    jne         mainloop

; last pixel
    movq        mm2,mmword ptr [ebx]
    movq        mm3,mm2
    pshufw      mm5,mm2,4Eh
    pcmpeqd     mm3,mmword ptr [ecx]
    movq        mm7,mm0
    pshufw      mm4,mm3,4Eh
    punpckhdq   mm7,mm1
    pcmpeqd     mm7,mm5
    pandn       mm4,mm7
    pshufw      mm6,mm7,4Eh
    pandn       mm6,mm4
    pshufw      mm7,mm1,44h
    pand        mm5,mm6
    pandn       mm6,mm7
    pshufw      mm0,mm1,0EEh
    por         mm5,mm6
    pshufw      mm7,mm2,0EEh
    movq        mm2,mm1
    pshufw      mm4,mm3,0EEh
    punpckldq   mm2,mm0
    pcmpeqd     mm2,mm7
    pandn       mm4,mm2
    pshufw      mm6,mm2,4Eh
    pandn       mm6,mm4
    pshufw      mm3,mm1,0EEh
    pand        mm7,mm6
    pandn       mm6,mm3
    por         mm7,mm6
    movntq      mmword ptr [edx],mm5
    movntq      mmword ptr [edx+8],mm7
    emms
    ret
Scale2xScaler_scaleLineHalf_1on2_4_SSE ENDP

end