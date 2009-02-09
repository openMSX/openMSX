INCLUDE macamd64.inc

.code

; FBPostProcessor_drawNoiseLine_4_SSE2
;   rcx = Pixel* in
;   rdx = Pixel* out
;   r8 = signed char* noise
;   r9 = unsigned long width
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY FBPostProcessor_drawNoiseLine_4_SSE2, Video

; rcx = in + width
; rdx = out + width
; r8 = noise + 4 * width
; r9 = -4 * width
    shl         r9,2
    lea         rcx,[rcx+r9]
    lea         rdx,[rdx+r9]
    lea         r8,[r8+r9]
    neg         r9
    
    pcmpeqb     xmm7,xmm7
    psllw       xmm7,0Fh
    packsswb    xmm7,xmm7

mainloop:
    movdqa      xmm0,xmmword ptr [rcx+r9]
    movdqa      xmm1,xmmword ptr [rcx+r9+10h]
    movdqa      xmm2,xmmword ptr [rcx+r9+20h]
    pxor        xmm0,xmm7
    movdqa      xmm3,xmmword ptr [rcx+r9+30h]
    pxor        xmm1,xmm7
    pxor        xmm2,xmm7
    paddsb      xmm0,xmmword ptr [r8+r9]
    pxor        xmm3,xmm7
    paddsb      xmm1,xmmword ptr [r8+r9+10h]
    paddsb      xmm2,xmmword ptr [r8+r9+20h]
    pxor        xmm0,xmm7
    paddsb      xmm3,xmmword ptr [r8+r9+30h]
    pxor        xmm1,xmm7
    pxor        xmm2,xmm7
    movdqa      xmmword ptr [rdx+r9],xmm0
    pxor        xmm3,xmm7
    movdqa      xmmword ptr [rdx+r9+10h],xmm1
    movdqa      xmmword ptr [rdx+r9+20h],xmm2
    movdqa      xmmword ptr [rdx+r9+30h],xmm3
    add         r9,40h
    jne         mainloop
    ret    
LEAF_END FBPostProcessor_drawNoiseLine_4_SSE2, Video

end