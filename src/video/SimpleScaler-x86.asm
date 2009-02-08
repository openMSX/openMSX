.686
.XMM
.model flat, c

.code

; SimpleScaler_blur1on2_4_MMX
;   const Pixel* pIn
;   Pixel* pOut
;   unsigned srcWidth
;   unsigned c1
;   unsigned c2
; Preserved registers: edi, esi, ebp, ebx
SimpleScaler_blur1on2_4_MMX PROC C uses edi esi ebx pIn:NEAR PTR, pOut:NEAR PTR, srcWidth:DWORD, c1:DWORD, c2:DWORD

; ecx = pIn + srcWidth - 2
; edx = pOut + 2 * (srcWidth - 2)
; ebx = c1
; esi = c2
; edi = -4 * (srcWidth - 2)
    mov     edi,srcWidth
    sub     edi,2
    shl     edi,2
    mov     ecx,pIn
    lea     ecx,[ecx+edi]
    shl     edi,1
    mov     edx,pOut
    lea     edx,[edx+edi]
    mov     ebx,c1
    mov     esi,c2
    shr     edi,1
    neg     edi

    movd        mm5,ebx
    punpcklwd   mm5,mm5
    punpckldq   mm5,mm5
    movd        mm6,esi
    punpcklwd   mm6,mm6

    punpckldq   mm6,mm6
    pxor        mm7,mm7
    movd        mm0,dword ptr [ecx+edi]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    movq        mm3,mm2

mainloop:
    pmullw      mm0,mm6
    movq        mm4,mm0
    paddw       mm0,mm3
    psrlw       mm0,8
    movd        mm1,dword ptr [ecx+edi+4]
    punpcklbw   mm1,mm7
    movq        mm3,mm1

    pmullw      mm3,mm5
    paddw       mm4,mm3
    psrlw       mm4,8
    packuswb    mm0,mm4
    movq        mmword ptr [edx+edi*2],mm0
    pmullw      mm1,mm6
    movq        mm4,mm1
    paddw       mm1,mm2

    psrlw       mm1,8
    movd        mm0,dword ptr [ecx+edi+8]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    paddw       mm4,mm2
    psrlw       mm4,8
    packuswb    mm1,mm4

    movq        mmword ptr [edx+edi*2+8],mm1
    add         edi,8
    jne         mainloop

    pmullw      mm0,mm6
    movq        mm4,mm0
    paddw       mm0,mm3
    psrlw       mm0,8
    movd        mm1,dword ptr [ecx+4]

    punpcklbw   mm1,mm7
    movq        mm3,mm1
    pmullw      mm3,mm5
    paddw       mm4,mm3
    psrlw       mm4,8
    packuswb    mm0,mm4
    movq        mmword ptr [edx],mm0
    movq        mm4,mm1

    pmullw      mm1,mm6
    paddw       mm1,mm2
    psrlw       mm1,8
    packuswb    mm1,mm4
    movq        mmword ptr [edx+8],mm1
    emms
    ret
SimpleScaler_blur1on2_4_MMX ENDP

end
