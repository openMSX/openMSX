INCLUDE macamd64.inc

.code

; Blur_1on3_4_SSE
;   rcx = const Pixel* in
;   rdx = Pixel* out
;   r8 = unsigned long dstWidth
;   r9 = struct* &c
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Blur_1on3_4_SSE, Video

    sub         r8,6
    shl         r8,2
; r10 = out + (dstWidth - 6)
    lea         r10,[rdx+r8]
; r8 = (-4 * (dstWidth - 6))
    neg         r8

    pxor        mm0,mm0
    pshufw      mm1,mmword ptr [r9+28h],0
    pshufw      mm2,mmword ptr [r9+2Ch],0
    pshufw      mm3,mmword ptr [r9+30h],0
    pshufw      mm4,mmword ptr [r9+34h],0
    movq        mmword ptr [r9],mm0
    movq        mmword ptr [r9+8],mm1
    movq        mmword ptr [r9+10h],mm2
    movq        mmword ptr [r9+18h],mm3
    movq        mmword ptr [r9+20h],mm4
    movq        mm0,mmword ptr [rcx]
    movq        mm5,mm0
    punpcklbw   mm0,dword ptr [r9]
    movq        mm2,mm0
    pmulhuw     mm2,mmword ptr [r9+8]
    movq        mm3,mm0
    pmulhuw     mm3,mmword ptr [r9+10h]
    movq        mm4,mm2
    movq        mm6,mm3

mainloop:
    prefetchnta [rcx+0C0h]
    prefetcht0  [r10+r8+140h]
    movq        mm1,mm5
    movq        mm7,mm0
    punpckhbw   mm1,mmword ptr [r9]
    pmulhuw     mm7,mmword ptr [r9+20h]
    paddw       mm7,mm2
    movq        mm2,mm1
    pmulhuw     mm0,mmword ptr [r9+18h]
    pmulhuw     mm2,mmword ptr [r9+8]
    movq        mm5,mmword ptr [rcx+8]
    paddw       mm3,mm0
    paddw       mm7,mm2
    packuswb    mm3,mm7
    movq        mmword ptr [r10+r8],mm3
    movq        mm3,mm1
    movq        mm7,mm1
    pmulhuw     mm3,mmword ptr [r9+10h]
    pmulhuw     mm7,mmword ptr [r9+18h]
    paddw       mm0,mm3
    paddw       mm6,mm7
    packuswb    mm0,mm6
    movq        mmword ptr [r10+r8+8],mm0
    movq        mm0,mm5
    pmulhuw     mm1,mmword ptr [r9+20h]
    punpcklbw   mm0,dword ptr [r9]
    paddw       mm1,mm4
    movq        mm6,mm0
    movq        mm4,mm0
    pmulhuw     mm6,mmword ptr [r9+10h]
    pmulhuw     mm4,mmword ptr [r9+8]
    paddw       mm7,mm6
    paddw       mm1,mm4
    packuswb    mm1,mm7
    add         rcx,8
    movq        mmword ptr [r10+r8+10h],mm1
    add         r8,18h
    jne         mainloop

    movq        mm1,mm5
    movq        mm7,mm0
    punpckhbw   mm1,mmword ptr [r9]
    pmulhuw     mm7,mmword ptr [r9+20h]
    paddw       mm7,mm2
    movq        mm2,mm1
    pmulhuw     mm0,mmword ptr [r9+18h]
    pmulhuw     mm2,mmword ptr [r9+8]
    paddw       mm3,mm0
    paddw       mm7,mm2
    packuswb    mm3,mm7
    movq        mmword ptr [r10],mm3
    movq        mm3,mm1
    movq        mm7,mm1
    pmulhuw     mm3,mmword ptr [r9+10h]
    pmulhuw     mm7,mmword ptr [r9+18h]
    paddw       mm0,mm3
    paddw       mm6,mm7
    packuswb    mm0,mm6
    movq        mmword ptr [r10+8],mm0
    movq        mm7,mm1
    pmulhuw     mm1,mmword ptr [r9+20h]
    paddw       mm1,mm4
    paddw       mm1,mm2
    packuswb    mm1,mm7
    movq        mmword ptr [r10+10h],mm1
    emms
    ret
LEAF_END Blur_1on3_4_SSE, Video

end