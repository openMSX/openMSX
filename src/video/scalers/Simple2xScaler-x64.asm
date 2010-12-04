INCLUDE macamd64.inc

.code

; Simple2xScaler_blur1on2_4_MMX
;   rcx = const Pixel* pIn
;   rdx = Pixel* pOut
;   r8 = unsigned srcWidth
;   r9 = unsigned c1
;   [rsp+28h] = unsigned c2
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Simple2xScaler_blur1on2_4_MMX, Video

; rcx = pIn + srcWidth - 2
; rdx = pOut + 2 * (srcWidth - 2)
; r9 = c1
; r10 = c2
; r8 = -4 * (srcWidth - 2)
    sub         r8,2
    shl         r8,2
    lea         rcx,[rcx+r8]
    shl         r8,1
    lea         rdx,[rdx+r8]
    mov         r10d,dword ptr[rsp+28h]
    shr         r8,1
    neg         r8

    movd        mm5,r9
    punpcklwd   mm5,mm5
    punpckldq   mm5,mm5
    movd        mm6,r10
    punpcklwd   mm6,mm6

    punpckldq   mm6,mm6
    pxor        mm7,mm7
    movd        mm0,dword ptr [rcx+r8]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    movq        mm3,mm2

mainloop:
    pmullw      mm0,mm6
    movq        mm4,mm0
    paddw       mm0,mm3
    psrlw       mm0,8
    movd        mm1,dword ptr [rcx+r8+4]
    punpcklbw   mm1,mm7
    movq        mm3,mm1

    pmullw      mm3,mm5
    paddw       mm4,mm3
    psrlw       mm4,8
    packuswb    mm0,mm4
    movq        mmword ptr [rdx+r8*2],mm0
    pmullw      mm1,mm6
    movq        mm4,mm1
    paddw       mm1,mm2

    psrlw       mm1,8
    movd        mm0,dword ptr [rcx+r8+8]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    paddw       mm4,mm2
    psrlw       mm4,8
    packuswb    mm1,mm4

    movq        mmword ptr [rdx+r8*2+8],mm1
    add         r8,8
    jne         mainloop

    pmullw      mm0,mm6
    movq        mm4,mm0
    paddw       mm0,mm3
    psrlw       mm0,8
    movd        mm1,dword ptr [rcx+4]

    punpcklbw   mm1,mm7
    movq        mm3,mm1
    pmullw      mm3,mm5
    paddw       mm4,mm3
    psrlw       mm4,8
    packuswb    mm0,mm4
    movq        mmword ptr [rdx],mm0
    movq        mm4,mm1

    pmullw      mm1,mm6
    paddw       mm1,mm2
    psrlw       mm1,8
    packuswb    mm1,mm4
    movq        mmword ptr [rdx+8],mm1
    emms
    ret
LEAF_END Simple2xScaler_blur1on2_4_MMX, Video

; Simple2xScaler_blur1on1_4_MMX
;   rcx = const Pixel* pIn
;   rdx = Pixel* pOut
;   r8 = unsigned srcWidth
;   r9 = unsigned c1
;   [rsp+28h] = unsigned c2
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Simple2xScaler_blur1on1_4_MMX, Video

; rcx = pIn + srcWidth - 2
; rdx = pOut + srcWidth - 2
; r9 = c1
; r10 = c2
; r8 = -4 * (srcWidth - 2)
    sub         r8,2
    shl         r8,2
    lea         rcx,[rcx+r8]
    lea         rdx,[rdx+r8]
    mov         r10d,dword ptr [rsp+28h]
    neg         r8

    movd        mm5,r9
    punpcklwd   mm5,mm5
    punpckldq   mm5,mm5
    movd        mm6,r10
    punpcklwd   mm6,mm6
    punpckldq   mm6,mm6
    pxor        mm7,mm7
    movd        mm0,dword ptr [rcx+r8]
    punpcklbw   mm0,mm7
    movq        mm2,mm0
    pmullw      mm2,mm5
    movq        mm3,mm2

mainloop:
    movd        mm1,dword ptr [rcx+r8+4]
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
    movd        mm0,dword ptr [rcx+r8+8]
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
    movq        mmword ptr [rdx+r8],mm4
    add         r8,8
    jne         mainloop

    movd        mm1,dword ptr [rcx+4]
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
    movq        mmword ptr [rdx],mm4
    emms
    ret
LEAF_END Simple2xScaler_blur1on1_4_MMX, Video

end
