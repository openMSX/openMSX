.686
.XMM
.model flat, c

.code

; Blur_1on3_4_SSE
;   const Pixel* in
;   Pixel* out
;   unsigned long dstWidth
;   &c
; Preserved registers: edi, esi, ebp, ebx
Blur_1on3_4_SSE PROC C uses ebx _in:NEAR PTR, _out:NEAR PTR, dstWidth:DWORD, _c:NEAR PTR

; edx = in
; ebx = out + (dstWidth - 6)
; ecx = -4 * (dstWidth - 6)
; eax = c
    mov         edx,_in
    mov         ecx,dstWidth
    sub         ecx,6
    shl         ecx,2
    mov         ebx,_out
    lea         ebx,[ebx+ecx]
    neg         ecx
    mov         eax,_c

    pxor        mm0,mm0
    pshufw      mm1,mmword ptr [eax+28h],0
    pshufw      mm2,mmword ptr [eax+2Ch],0
    pshufw      mm3,mmword ptr [eax+30h],0
    pshufw      mm4,mmword ptr [eax+34h],0
    movq        mmword ptr [eax],mm0
    movq        mmword ptr [eax+8],mm1
    movq        mmword ptr [eax+10h],mm2
    movq        mmword ptr [eax+18h],mm3
    movq        mmword ptr [eax+20h],mm4
    movq        mm0,mmword ptr [edx]
    movq        mm5,mm0
    punpcklbw   mm0,dword ptr [eax]
    movq        mm2,mm0
    pmulhuw     mm2,mmword ptr [eax+8]
    movq        mm3,mm0
    pmulhuw     mm3,mmword ptr [eax+10h]
    movq        mm4,mm2
    movq        mm6,mm3

mainloop:
    prefetchnta [edx+0C0h]
    prefetcht0  [ebx+ecx+140h]
    movq        mm1,mm5
    movq        mm7,mm0
    punpckhbw   mm1,mmword ptr [eax]
    pmulhuw     mm7,mmword ptr [eax+20h]
    paddw       mm7,mm2
    movq        mm2,mm1
    pmulhuw     mm0,mmword ptr [eax+18h]
    pmulhuw     mm2,mmword ptr [eax+8]
    movq        mm5,mmword ptr [edx+8]
    paddw       mm3,mm0
    paddw       mm7,mm2
    packuswb    mm3,mm7
    movq        mmword ptr [ebx+ecx],mm3
    movq        mm3,mm1
    movq        mm7,mm1
    pmulhuw     mm3,mmword ptr [eax+10h]
    pmulhuw     mm7,mmword ptr [eax+18h]
    paddw       mm0,mm3
    paddw       mm6,mm7
    packuswb    mm0,mm6
    movq        mmword ptr [ebx+ecx+8],mm0
    movq        mm0,mm5
    pmulhuw     mm1,mmword ptr [eax+20h]
    punpcklbw   mm0,dword ptr [eax]
    paddw       mm1,mm4
    movq        mm6,mm0
    movq        mm4,mm0
    pmulhuw     mm6,mmword ptr [eax+10h]
    pmulhuw     mm4,mmword ptr [eax+8]
    paddw       mm7,mm6
    paddw       mm1,mm4
    packuswb    mm1,mm7
    add         edx,8
    movq        mmword ptr [ebx+ecx+10h],mm1
    add         ecx,18h
    jne         mainloop

    movq        mm1,mm5
    movq        mm7,mm0
    punpckhbw   mm1,mmword ptr [eax]
    pmulhuw     mm7,mmword ptr [eax+20h]
    paddw       mm7,mm2
    movq        mm2,mm1
    pmulhuw     mm0,mmword ptr [eax+18h]
    pmulhuw     mm2,mmword ptr [eax+8]
    paddw       mm3,mm0
    paddw       mm7,mm2
    packuswb    mm3,mm7
    movq        mmword ptr [ebx],mm3
    movq        mm3,mm1
    movq        mm7,mm1
    pmulhuw     mm3,mmword ptr [eax+10h]
    pmulhuw     mm7,mmword ptr [eax+18h]
    paddw       mm0,mm3
    paddw       mm6,mm7
    packuswb    mm0,mm6
    movq        mmword ptr [ebx+8],mm0
    movq        mm7,mm1
    pmulhuw     mm1,mmword ptr [eax+20h]
    paddw       mm1,mm4
    paddw       mm1,mm2
    packuswb    mm1,mm7
    movq        mmword ptr [ebx+10h],mm1
    emms
    ret
Blur_1on3_4_SSE ENDP

end