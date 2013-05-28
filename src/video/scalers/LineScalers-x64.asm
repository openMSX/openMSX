INCLUDE macamd64.inc

.code

; Scale_1on2_4_MMX
;   rcx = const Pixel* in
;   rdx = Pixel* out
;   r8 = unsigned long width
; Scratch registers: rax, rcx, rdx, r8, r9, r10, and r11
LEAF_ENTRY Scale_1on2_4_MMX, Video

; rcx = in + width / 2
; rdx = out + width
; r8 = -2 * width
    shl         r8,2
    lea         rdx,[rdx+r8]
    shr         r8,1
    lea         rcx,[rcx+r8]
    neg         r8
mainloop:
; Load
    movq	    mm0,mmword ptr [rcx+r8]
	movq	    mm2,mmword ptr [rcx+r8+8]
	movq	    mm4,mmword ptr [rcx+r8+10h]
	movq	    mm6,mmword ptr [rcx+r8+18h]
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
    movq        mmword ptr [rdx+r8*2],mm0
    movq        mmword ptr [rdx+r8*2+8],mm1
    movq        mmword ptr [rdx+r8*2+10h],mm2
    movq        mmword ptr [rdx+r8*2+18h],mm3
    movq        mmword ptr [rdx+r8*2+20h],mm4
    movq        mmword ptr [rdx+r8*2+28h],mm5
    movq        mmword ptr [rdx+r8*2+30h],mm6
    movq        mmword ptr [rdx+r8*2+38h],mm7
; Increment
	add	        r8,20h
	jne	        mainloop
	emms
	ret
LEAF_END Scale_1on2_4_MMX, Video

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
