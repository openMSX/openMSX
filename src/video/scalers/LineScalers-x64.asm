INCLUDE macamd64.inc

.code

; Scale_2on1_SSE
;   rcx = const Pixel* in
;   rdx = Pixel* out
;   r8 = unsigned long width
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Scale_2on1_SSE, Video

; rcx = in + 2 * width
; rdx = out + width
; r8 = -4 * width
    shl         r8,3
    lea         rcx,[rcx+r8]
    shr         r8,1
    lea         rdx,[rdx+r8]
    neg         r8

mainloop:
    movq        mm0,mmword ptr [rcx+r8*2]
    movq        mm1,mmword ptr [rcx+r8*2+8]
    movq        mm2,mmword ptr [rcx+r8*2+10h]
    movq        mm3,mmword ptr [rcx+r8*2+18h]
    movq        mm4,mm0
    punpckhdq   mm0,mm1
    punpckldq   mm4,mm1
    movq        mm5,mm2
    punpckhdq   mm2,mm3
    punpckldq   mm5,mm3
    pavgb       mm4,mm0
    movntq      mmword ptr [rdx+r8],mm4
    pavgb       mm5,mm2
    movntq      mmword ptr [rdx+r8+8],mm5
    add         r8,10h
    jne         mainloop
    emms
    ret
LEAF_END Scale_2on1_SSE, Video

end
