.686
.XMM
.model flat, c

.code

; Scanline_draw_4_SSE2
;   const Pixel* src1
;   const Pixel* src2
;   Pixel* dst
;   unsigned factor
;   unsigned long width
; Preserved registers: edi, esi, ebp, ebx
Scanline_draw_4_SSE2 PROC C uses edi esi ebx src1:NEAR PTR, src2:NEAR PTR, dst:NEAR PTR, factor:DWORD, _width:DWORD

; ebx = src1 + width;
; ecx = src2 + width;
; edx = dst + width;
; esi = factor << 8;
; eax = -4 * width;
    mov         esi,factor
    shl         esi,8
    mov         ecx,src1
    mov         edx,src2
    mov         edi,dst
    mov         eax,_width
    shl         eax,2 
    lea         ebx,[eax+ecx] 
    lea         ecx,[eax+edx] 
    lea         edx,[eax+edi] 
    neg         eax  

    movd        xmm6,esi 
    pshuflw     xmm6,xmm6,0 
    pxor        xmm7,xmm7 
    pshufd      xmm6,xmm6,0 

mainloop:
    movdqa      xmm0,xmmword ptr [ebx+eax] 
    pavgb       xmm0,xmmword ptr [ecx+eax] 
    movdqa      xmm4,xmm0 
    punpcklbw   xmm0,xmm7 
    punpckhbw   xmm4,xmm7 
    pmulhuw     xmm0,xmm6 
    pmulhuw     xmm4,xmm6 
    packuswb    xmm0,xmm4 
    movdqa      xmm1,xmmword ptr [ebx+eax+10h] 
    pavgb       xmm1,xmmword ptr [ecx+eax+10h] 
    movdqa      xmm5,xmm1 
    punpcklbw   xmm1,xmm7 
    punpckhbw   xmm5,xmm7 
    pmulhuw     xmm1,xmm6 
    pmulhuw     xmm5,xmm6 
    packuswb    xmm1,xmm5 
    movdqa      xmm2,xmmword ptr [ebx+eax+20h] 
    pavgb       xmm2,xmmword ptr [ecx+eax+20h] 
    movdqa      xmm4,xmm2 
    punpcklbw   xmm2,xmm7 
    punpckhbw   xmm4,xmm7 
    pmulhuw     xmm2,xmm6 
    pmulhuw     xmm4,xmm6 
    packuswb    xmm2,xmm4 
    movdqa      xmm3,xmmword ptr [ebx+eax+30h] 
    pavgb       xmm3,xmmword ptr [ecx+eax+30h] 
    movdqa      xmm5,xmm3 
    punpcklbw   xmm3,xmm7 
    punpckhbw   xmm5,xmm7 
    pmulhuw     xmm3,xmm6 
    pmulhuw     xmm5,xmm6 
    packuswb    xmm3,xmm5 
    movntps     xmmword ptr [edx+eax],xmm0 
    movntps     xmmword ptr [edx+eax+10h],xmm1 
    movntps     xmmword ptr [edx+eax+20h],xmm2 
    movntps     xmmword ptr [edx+eax+30h],xmm3 
    add         eax,40h
    jne         mainloop
    ret
Scanline_draw_4_SSE2 ENDP

end