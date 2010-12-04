INCLUDE macamd64.inc

.code

; Scanline_draw_4_SSE2
;   rcx = const Pixel* src1
;   rdx = const Pixel* src2
;   r8 = Pixel* dst
;   r9 = unsigned factor
;   [rsp+28h] = unsigned long width
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Scanline_draw_4_SSE2, Video

; rax = width * 4
    mov         eax,dword ptr[rsp+28h]
    shl         rax,2  
; r10 = src1 + width;
; rcx = src2 + width;
; rdx = dst + width;
; r9 = factor << 8;
; rax = -4 * width;
    lea         r10,[rcx+rax]
    lea         rcx,[rdx+rax]
    lea         rdx,[r8+rax]
    shl         r9,8
    neg         rax

    movd        xmm6,r9
    pshuflw     xmm6,xmm6,0 
    pxor        xmm7,xmm7 
    pshufd      xmm6,xmm6,0 

mainloop:
    movdqa      xmm0,xmmword ptr [r10+rax] 
    pavgb       xmm0,xmmword ptr [rcx+rax] 
    movdqa      xmm4,xmm0 
    punpcklbw   xmm0,xmm7 
    punpckhbw   xmm4,xmm7 
    pmulhuw     xmm0,xmm6 
    pmulhuw     xmm4,xmm6 
    packuswb    xmm0,xmm4 
    movdqa      xmm1,xmmword ptr [r10+rax+10h] 
    pavgb       xmm1,xmmword ptr [rcx+rax+10h] 
    movdqa      xmm5,xmm1 
    punpcklbw   xmm1,xmm7 
    punpckhbw   xmm5,xmm7 
    pmulhuw     xmm1,xmm6 
    pmulhuw     xmm5,xmm6 
    packuswb    xmm1,xmm5 
    movdqa      xmm2,xmmword ptr [r10+rax+20h] 
    pavgb       xmm2,xmmword ptr [rcx+rax+20h] 
    movdqa      xmm4,xmm2 
    punpcklbw   xmm2,xmm7 
    punpckhbw   xmm4,xmm7 
    pmulhuw     xmm2,xmm6 
    pmulhuw     xmm4,xmm6 
    packuswb    xmm2,xmm4 
    movdqa      xmm3,xmmword ptr [r10+rax+30h] 
    pavgb       xmm3,xmmword ptr [rcx+rax+30h] 
    movdqa      xmm5,xmm3 
    punpcklbw   xmm3,xmm7 
    punpckhbw   xmm5,xmm7 
    pmulhuw     xmm3,xmm6 
    pmulhuw     xmm5,xmm6 
    packuswb    xmm3,xmm5 
    movntps     xmmword ptr [rdx+rax],xmm0 
    movntps     xmmword ptr [rdx+rax+10h],xmm1 
    movntps     xmmword ptr [rdx+rax+20h],xmm2 
    movntps     xmmword ptr [rdx+rax+30h],xmm3 
    add         rax,40h
    jne         mainloop
    ret
LEAF_END Scanline_draw_4_SSE2, Video

end