INCLUDE macamd64.inc

.code

; Scale2xScaler_scaleLineHalf_1on2_4_SSE
;   rcx = Pixel* dst
;   rdx = const Pixel* src0
;   r8 = const Pixel* src1
;   r9 = const Pixel* src2
;   [rsp+28h] = unsigned long srcWidth
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Scale2xScaler_scaleLineHalf_1on2_4_SSE, Video

; r8 = src1 + srcWidth - 2
; rdx = src0 + srcWidth - 2
; r9 = src2 + srcWidth - 2
; rcx = dst + 2 * (srcWidth - 2)
; rax = -4 * (srcWidth - 2)
    mov         eax,dword ptr [rsp+28h]
    sub         rax,2
    shl         rax,2
    lea         r8,[r8+rax]
    lea         rdx,[rdx+rax]
    lea         r9,[r9+rax]
    lea         rcx,[rcx+2*rax]
    neg         rax

    movq        mm1,mmword ptr [r8+rax]
    pshufw      mm0,mm1,0EEh

mainloop:
    movq        mm2,mmword ptr [rdx+rax]
    movq        mm3,mm2
    pshufw      mm5,mm2,44h
    pcmpeqd     mm3,mmword ptr [r9+rax]
    movq        mm7,mm0
    pshufw      mm4,mm3,44h
    punpckhdq   mm7,mm1
    pcmpeqd     mm7,mm5
    pandn       mm4,mm7
    pshufw      mm6,mm7,4Eh
    pandn       mm6,mm4
    pshufw      mm7,mm1,44h
    pand        mm5,mm6
    movq        mm0,mmword ptr [r8+rax+8]
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
    movntq      mmword ptr [rcx+rax*2],mm5
    movntq      mmword ptr [rcx+rax*2+8],mm7
    add         rax,8
    jne         mainloop

; last pixel
    movq        mm2,mmword ptr [rdx]
    movq        mm3,mm2
    pshufw      mm5,mm2,4Eh
    pcmpeqd     mm3,mmword ptr [r9]
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
    movntq      mmword ptr [rcx],mm5
    movntq      mmword ptr [rcx+8],mm7
    emms
    ret
LEAF_END Scale2xScaler_scaleLineHalf_1on2_4_SSE, Video

end