.686
.XMM
.model flat, c

.code

; Scale_2on1_SSE
;   const Pixel* in
;   Pixel* out
;   unsigned long width
; Preserved registers: edi, esi, ebp, ebx
Scale_2on1_SSE PROC C _in:NEAR PTR, _out:NEAR PTR, _width:DWORD

; eax = in + 2 * width
; ecx = out + width
; edx = -4 * width
    mov         edx,_width
    shl         edx,3
    mov         eax,_in
    lea         eax,[eax+edx]
    shr         edx,1
    mov         ecx,_out
    lea         ecx,[ecx+edx]
    neg         edx

mainloop:
    movq        mm0,mmword ptr [eax+edx*2]
    movq        mm1,mmword ptr [eax+edx*2+8]
    movq        mm2,mmword ptr [eax+edx*2+10h]
    movq        mm3,mmword ptr [eax+edx*2+18h]
    movq        mm4,mm0
    punpckhdq   mm0,mm1
    punpckldq   mm4,mm1
    movq        mm5,mm2
    punpckhdq   mm2,mm3
    punpckldq   mm5,mm3
    pavgb       mm4,mm0
    movntq      mmword ptr [ecx+edx],mm4
    pavgb       mm5,mm2
    movntq      mmword ptr [ecx+edx+8],mm5
    add         edx,10h
    jne         mainloop
    emms
    ret
Scale_2on1_SSE ENDP

end
