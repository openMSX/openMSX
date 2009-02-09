.686
.XMM
.model flat, c

.code

; FBPostProcessor_drawNoiseLine_4_SSE2
;   Pixel* in
;   Pixel* out
;   signed char* noise
;   unsigned long width
; Preserved registers: edi, esi, ebp, ebx
FBPostProcessor_drawNoiseLine_4_SSE2 PROC C uses ebx _in:NEAR PTR, _out:NEAR PTR, noise:NEAR PTR, _width:DWORD

; ebx = in + width
; ecx = out + width
; edx = noise + 4 * width
; eax = -4 * width
    mov         eax,_width
    shl         eax,2
    mov         ebx,_in
    lea         ebx,[ebx+eax]
    mov         ecx,_out
    lea         ecx,[ecx+eax]
    mov         edx,noise
    lea         edx,[edx+eax]
    neg         eax
    
    pcmpeqb     xmm7,xmm7
    psllw       xmm7,0Fh
    packsswb    xmm7,xmm7

mainloop:
    movdqa      xmm0,xmmword ptr [ebx+eax]
    movdqa      xmm1,xmmword ptr [ebx+eax+10h]
    movdqa      xmm2,xmmword ptr [ebx+eax+20h]
    pxor        xmm0,xmm7
    movdqa      xmm3,xmmword ptr [ebx+eax+30h]
    pxor        xmm1,xmm7
    pxor        xmm2,xmm7
    paddsb      xmm0,xmmword ptr [edx+eax]
    pxor        xmm3,xmm7
    paddsb      xmm1,xmmword ptr [edx+eax+10h]
    paddsb      xmm2,xmmword ptr [edx+eax+20h]
    pxor        xmm0,xmm7
    paddsb      xmm3,xmmword ptr [edx+eax+30h]
    pxor        xmm1,xmm7
    pxor        xmm2,xmm7
    movdqa      xmmword ptr [ecx+eax],xmm0
    pxor        xmm3,xmm7
    movdqa      xmmword ptr [ecx+eax+10h],xmm1
    movdqa      xmmword ptr [ecx+eax+20h],xmm2
    movdqa      xmmword ptr [ecx+eax+30h],xmm3
    add         eax,40h
    jne         mainloop
    ret    
FBPostProcessor_drawNoiseLine_4_SSE2 ENDP

end