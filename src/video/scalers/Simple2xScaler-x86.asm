.686
.XMM
.model flat, c

.code

; Simple2xScaler_blur1on2_4_MMX
;   const Pixel* pIn
;   Pixel* pOut
;   unsigned srcWidth
;   unsigned c1
;   unsigned c2
; Preserved registers: edi, esi, ebp, ebx
Simple2xScaler_blur1on2_4_MMX PROC C uses edi esi ebx pIn:NEAR PTR, pOut:NEAR PTR, srcWidth:DWORD, c1:DWORD, c2:DWORD

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
Simple2xScaler_blur1on2_4_MMX ENDP

; Simple2xScaler_blur1on1_4_MMX
;   const Pixel* pIn
;   Pixel* pOut
;   unsigned srcWidth
;   unsigned c1
;   unsigned c2
; Preserved registers: edi, esi, ebp, ebx
Simple2xScaler_blur1on1_4_MMX PROC C uses edi esi ebx pIn:NEAR PTR, pOut:NEAR PTR, srcWidth:DWORD, c1:DWORD, c2:DWORD

; ecx = pIn + srcWidth - 2
; edx = pOut + srcWidth - 2
; ebx = c1
; edi = c2
; eax = -4 * (srcWidth - 2)
    mov         eax,srcWidth
    sub         eax,2
    shl         eax,2
    mov         ecx,pIn
    lea         ecx,[ecx+eax]
    mov         edx,pOut
    lea         edx,[edx+eax]
    mov         ebx,c1
    mov         edi,c2
    neg         eax

    movd        mm5,ebx
    punpcklwd   mm5,mm5
    punpckldq   mm5,mm5
    movd        mm6,edi
    punpcklwd   mm6,mm6
    punpckldq   mm6,mm6
    pxor        mm7,mm7
    movd        mm0,dword ptr [ecx+eax]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    movq        mm3,mm2

mainloop:
    movd        mm1,dword ptr [ecx+eax+4]
    pxor        mm7,mm7
    punpcklbw   mm1,mm7
    movq        mm4,mm0
    pmullw      mm4,mm6
    movq        mm0,mm1
    pmullw      mm0,mm5
    paddw       mm4,mm2
    paddw       mm4,mm0
    psrlw       mm4,8
    movq        mm2,mm0
    movd        mm0,dword ptr [ecx+eax+8]
    punpcklbw   mm0,mm7
    movq        mm7,mm1
    pmullw      mm7,mm6
    movq        mm1,mm0
    pmullw      mm1,mm5
    paddw       mm7,mm3
    paddw       mm7,mm1
    psrlw       mm7,8
    movq        mm3,mm1
    packuswb    mm4,mm7
    movq        mmword ptr [edx+eax],mm4
    add         eax,8
    jne         mainloop

    movd        mm1,dword ptr [ecx+4]
    pxor        mm7,mm7
    punpcklbw   mm1,mm7
    movq        mm4,mm0
    pmullw      mm4,mm6
    movq        mm0,mm1
    pmullw      mm0,mm5
    paddw       mm4,mm2
    paddw       mm4,mm0
    psrlw       mm4,8
    pmullw      mm1,mm6
    paddw       mm1,mm3
    paddw       mm1,mm0
    psrlw       mm1,8
    packuswb    mm4,mm1
    movq        mmword ptr [edx],mm4
    emms
    ret
Simple2xScaler_blur1on1_4_MMX ENDP

end
