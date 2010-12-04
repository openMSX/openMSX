.686
.XMM
.model flat, c

.code

; Scale_1on2_4_MMX
;   const Pixel* in
;   Pixel* out
;   unsigned long width
; Preserved registers: edi, esi, ebp, ebx
Scale_1on2_4_MMX PROC C _in:NEAR PTR, _out:NEAR PTR, _width:DWORD

; eax = in + width / 2
; ecx = out + width
; edx = -2 * width
    mov         edx,_width
    shl         edx,2
    mov         ecx,_out
    lea         ecx,[ecx+edx]
    shr         edx,1
    mov         eax,_in
    lea         eax,[eax+edx]
    neg         edx
mainloop:
; Load
    movq	    mm0,mmword ptr [eax+edx]
	movq	    mm2,mmword ptr [eax+edx+8]
	movq	    mm4,mmword ptr [eax+edx+10h]
	movq	    mm6,mmword ptr [eax+edx+18h]
	movq	    mm1,mm0
	movq	    mm3,mm2
	movq	    mm5,mm4
	movq	    mm7,mm6
; Scale
    punpckldq   mm0,mm0
    punpckhdq   mm1,mm1
    punpckldq   mm2,mm2
    punpckhdq   mm3,mm3
    punpckldq   mm4,mm4
    punpckhdq   mm5,mm5
    punpckldq   mm6,mm6
    punpckhdq   mm7,mm7
; Store
    movq        mmword ptr [ecx+edx*2],mm0
    movq        mmword ptr [ecx+edx*2+8],mm1
    movq        mmword ptr [ecx+edx*2+10h],mm2
    movq        mmword ptr [ecx+edx*2+18h],mm3
    movq        mmword ptr [ecx+edx*2+20h],mm4
    movq        mmword ptr [ecx+edx*2+28h],mm5
    movq        mmword ptr [ecx+edx*2+30h],mm6
    movq        mmword ptr [ecx+edx*2+38h],mm7
; Increment
	add	        edx,20h
	jne	        mainloop
	emms
	ret
Scale_1on2_4_MMX ENDP

; Scale_1on1_SSE
;   const char* in
;   char* out
;   unsigned long nBytes
; Preserved registers: edi, esi, ebp, ebx
Scale_1on1_SSE PROC C _in:NEAR PTR, _out:NEAR PTR, nBytes:DWORD

; ecx = in + nBytes
; edx = out + nBytes
; eax = -nBytes
    mov         eax,nBytes
    mov         ecx,_in
    lea         ecx,[ecx+eax]
    mov         edx,_out
    lea         edx,[edx+eax]
    neg         eax

mainloop:
    movq        mm0,mmword ptr [ecx+eax]
    movq        mm1,mmword ptr [ecx+eax+8]
    movq        mm2,mmword ptr [ecx+eax+10h]
    movq        mm3,mmword ptr [ecx+eax+18h]
    movq        mm4,mmword ptr [ecx+eax+20h]
    movq        mm5,mmword ptr [ecx+eax+28h]
    movq        mm6,mmword ptr [ecx+eax+30h]
    movq        mm7,mmword ptr [ecx+eax+38h]
    movntq      mmword ptr [edx+eax],mm0
    movntq      mmword ptr [edx+eax+8],mm1
    movntq      mmword ptr [edx+eax+10h],mm2
    movntq      mmword ptr [edx+eax+18h],mm3
    movntq      mmword ptr [edx+eax+20h],mm4
    movntq      mmword ptr [edx+eax+28h],mm5
    movntq      mmword ptr [edx+eax+30h],mm6
    movntq      mmword ptr [edx+eax+38h],mm7
    add         eax,40h
    jne         mainloop
    emms
    ret
Scale_1on1_SSE ENDP

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