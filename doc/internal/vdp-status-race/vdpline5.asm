; VDPLINE (version 5, for YJK: EFFECT 2 = the lines of genyjk.py): does YJK
; move the VDP's timing, or only its pixels? (V9958.) The VDPLINE frame sync
; (VR, then K HR samples: CNT), then 64 unrolled lines of exactly 228 cycles:
; COLB to R#7 in the right border and D1 cycles later COLA, in the left border
; of the next line, so a backdrop stripe edge lies in each border; 8 groups
; of 8 lines, the writes 1 cycle later per group. The caller sets SCREEN 8 or
; SCREEN 12 (R#25 YJK) for the whole run, so the HR samples are taken in that
; mode too, and draws pixel columns. A backdrop edge lies at a fixed place on
; the screen for a fixed write moment; the pixels move 4 to the right in
; SCREEN 12. If YJK also moves the timing (HR) by 4 pixels, the writes move
; along and the edges keep their distance to the columns in both screens; if
; YJK only moves the pixels, the edges are 4 pixels closer to the columns
; (to the left of them) in SCREEN 12.
; R18VAL and R25VAL (FFh: unchanged) are written at the start, with
; interrupts off; at the end R#18 is restored from RG18SAV and R#25 from
; RG25SAV (0 on an MSX2).
; Interrupts are off; space (checked once per frame) stops it.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle); VDPW = 1 adds
; 1 cycle to every VDP I/O (T9769, S1990), which the lines compensate.
; vdpyjkl.asm, vdpyjkm.asm and vdpyjkn.asm (SWEEP = 1, 2 or 3) assemble
; this with the 128 lines (16 groups) of yjkl.inc, yjkm.inc or yjkn.inc, at
; A000h instead of C000h (the parameters below then at A003h and on), as
; VDPYJKL.BIN, VDPYJKM.BIN and VDPYJKN.BIN.

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG18SAV	equ	0FFF1h
RG25SAV	equ	0FFFAh
K	equ	48		; HR samples

	IFNDEF SWEEP
	output	"VDPLIN5.BIN"
	org	0C000h - 7
	ELSE
	IF SWEEP = 1
	output	"VDPYJKL.BIN"
	ELSE
	IF SWEEP = 2
	output	"VDPYJKM.BIN"
	ELSE
	output	"VDPYJKN.BIN"
	ENDIF
	ENDIF
	org	0A000h - 7
	ENDIF
	db	0FEh
	dw	start, end, start

start:	jp	main
coarse:	dw	380		; C003h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C005h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C006h: 1 if VDP I/O has an extra wait cycle
nlines:	db	64		; C007h: lines with the stripe
cola:	db	4		; C008h: R#7 outside the stripe
colb:	db	15		; C009h: R#7 inside the stripe
oldcpu:	db	0		; C00Ah
marker:	db	0		; C00Bh: free for the caller (openMSX test script)
effect:	db	0		; C00Ch: 0 = write R#7, 1 = write a palette entry
palidx:	db	4		; C00Dh: EFFECT 1: the palette entry
pala:	dw	0117h		; C00Eh: EFFECT 1: outside the stripe: R/B byte, G byte
palb:	dw	0070h		; C010h: EFFECT 1: inside the stripe: R/B byte, G byte
wreg:	db	7		; C012h: EFFECT 0: the register written (7: backdrop, 23: vertical scroll)
r18val:	db	0FFh		; C013h: R#18 during the run (FFh: unchanged)
r25val:	db	0FFh		; C014h: R#25 during the run (FFh: unchanged)

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode
	call	CHGCPU
.notr:	ld	a,(vdpw)	; the line loop's padding: 10 cycles with the
	or	a		; extra wait (4 OUTs are 4 cycles longer), else 14
	ld	hl,0		; NOP, NOP
	jr	nz,.w
	ld	hl,2323h	; INC HL, INC HL
.w:	ld	(lines.pad),hl
	ld	a,(vdpw)	; without the extra wait, IN is 1 cycle shorter:
	or	a		; make the sample loop 1 cycle longer
	ld	a,44h		; NEG: 10 cycles
	jr	nz,.w2
	ld	a,57h		; LD A,I: 11 cycles
.w2:	ld	(frame.fix+1),a
	ld	hl,0		; the palette loop's padding: 20 cycles with the
	ld	de,0		; extra wait (8 VDP I/Os), else 28
	ld	a,(vdpw)
	or	a
	jr	nz,.w3
	ld	hl,2323h	; INC HL x 4
	ld	de,2323h
.w3:	ld	(plines.pad),hl
	ld	(plines.pad+2),de
	ld	a,(palidx)
	ld	(plines.i1+1),a
	ld	(plines.i2+1),a
	ld	hl,lines	; which loop the stubs go to
	ld	a,(effect)
	or	a
	jr	z,.w4
	ld	hl,plines
	cp	1
	jr	z,.w4
	ld	hl,yjk0		; EFFECT 2: the YJK lines, per VDPW
	ld	a,(vdpw)
	or	a
	jr	z,.w4
	ld	hl,yjk1
.w4:	ld	(toloop+1),hl
	ld	ix,table	; table[c] = sled entry, stub for a delay of c
	ld	b,K + 1
	ld	de,sledend	; sled entry: 5 cycles less per 5 cycles of delay
	ld	hl,stubs	; stub: one cycle more per step, 5 stubs
	ld	c,5
.tb:	ld	(ix+0),e
	ld	(ix+1),d
	ld	a,(hl)
	ld	(ix+2),a
	inc	hl
	ld	a,(hl)
	ld	(ix+3),a
	inc	hl
	inc	ix
	inc	ix
	inc	ix
	inc	ix
	dec	c
	jr	nz,.tb2
	ld	c,5
	ld	hl,stubs
	dec	de
.tb2:	djnz	.tb
	push	iy
	di
	ld	a,(r18val)	; R#18 and R#25 for this run (written here, with
	cp	0FFh		; interrupts off, so that BASIC's interrupt handler
	jr	z,.n18		; can't come between the two bytes)
	out	(99h),a
	ld	a,80h + 18
	out	(99h),a
.n18:	ld	a,(r25val)
	cp	0FFh
	jr	z,.rel
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
.rel:	in	a,(0AAh)	; wait until space is released (a previous
	and	0F0h		; screen may have been stopped with it)
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jr	nc,.rel
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a

frame:	in	a,(99h)		; wait until VR = 1 (vertical blanking)
	and	40h
	jr	z,frame
.w0:	in	a,(99h)		; wait until VR = 0 (display starts)
	and	40h
	jr	nz,.w0
	ld	a,(hrwait)	; move the samples close to the HR edge
	ld	e,a
.hw:	dec	e		; 18 cycles per loop
	jr	nz,.hw
	ld	bc,K * 256	; B = samples, C = count of HR=1
.smp:	in	a,(99h)		; one sample every 229 cycles
	rlca			; 5    bit 5 -> bit 0
	rlca			; 5
	rlca			; 5
	and	1		; 8
	add	a,c		; 5
	ld	c,a		; 5
	ld	e,8		; 8    padding: 8 + 7*18 + 13 + 5 + 7 + 10 = 169
.pad:	dec	e		; 5
	jr	nz,.pad		; 13 / 8
	nop			; 5
	inc	de		; 7
.fix:	neg			; 10 (or LD A,I, 11, see vdpw)
	djnz	.smp		; 14   total 229
	ld	a,c		; delay = fixed + CNT: the stripe stays at
	ld	(cnt),a		; the same place relative to HR
	ld	bc,(coarse)
.cd:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.cd
	ld	a,(cnt)		; constant time from here: look up the delay
	ld	l,a
	ld	h,0
	add	hl,hl
	add	hl,hl
	ld	de,table
	add	hl,de
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	inc	hl
	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a
	push	hl
	pop	ix		; IX = stub
	ex	de,hl		; HL = sled entry
	ld	a,(nlines)
	ld	b,a
	ld	c,99h
	ld	a,(effect)
	cp	1
	jr	z,.pal
	ld	a,(wreg)	; EFFECT 0: D = COLB, E = R#WREG, A = COLA
	or	80h
	ld	e,a
	ld	a,(colb)
	ld	d,a
	ld	a,(cola)
	jp	(hl)		; 5
.pal:	ld	de,(palb)	; EFFECT 1: D = COLB's R/B, E = its G,
	ld	a,d		; IYH/IYL = COLA's
	ld	d,e
	ld	e,a
	ld	iy,(pala)
	ld	a,iyl
	ld	iyl,iyh
	ld	iyh,a
	jp	(hl)		; 5
	ds	10,0		; NOP sled: 5 cycles per NOP
sledend:jp	(ix)		; 10
stub0:	inc	hl		; 14
	inc	hl
	jp	toloop
stub1:	nop			; 15
	nop
	nop
	jp	toloop
stub2:	nop			; 5 + 11 = 16
	jp	.s2
.s2:	jp	toloop
stub3:	nop			; 17
	nop
	inc	hl
	jp	toloop
stub4:	inc	hl		; 7 + 11 = 18
	jp	.s4
.s4:	jp	toloop

toloop:	jp	lines		; 11: LINES or PLINES, see EFFECT

lines:	out	(c),d		; 14/15  COLB
	out	(c),e		; 14/15  -> R#7
	ld	l,3		; 8      84 cycles: 8 + 2*18 + 13 + 4*5 + 7
.d1:	dec	l		; 5
	jr	nz,.d1		; 13 / 8
	nop
	nop
	nop
	nop
	inc	hl		; 7
	out	(c),a		; 14/15  COLA
	out	(c),e		; 14/15  -> R#7
	ld	l,2		; 8      60 cycles: 8 + 18 + 13 + 3*7
.d2:	dec	l
	jr	nz,.d2
	inc	hl
	inc	hl
	inc	hl
.pad:	nop			; 10 or 14, see vdpw
	nop
	djnz	lines		; 14   total 4*14 + 84 + 60 + 14 + 14 = 228
				;      (with VDPW 4*15 + ... + 10 + 14)
.end:	in	a,(0AAh)	; space bar: keyboard row 8, bit 0
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jp	c,frame
	pop	iy
	xor	a		; select S#0 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(RG18SAV)	; restore R#18
	out	(99h),a
	ld	a,80h + 18
	out	(99h),a
	ld	a,(MSXVER)	; and R#25: from RG25SAV on an MSX2+ or later,
	cp	2		; 0 on an MSX2
	ld	a,0
	jr	c,.w25
	ld	a,(RG25SAV)
.w25:	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; EFFECT 1: the same with palette writes (R#16 = PALIDX, then R/B and G to
; port 9Ah; the second one takes effect). D/E = COLB's R/B and G, IYH/IYL =
; COLA's.
plines:	ld	a,4		; 8      (PALIDX)
.i1	equ	$ - 2
	out	(99h),a		; 12/13
	ld	a,90h		; 8      -> R#16
	out	(99h),a		; 12/13
	ld	a,d		; 5
	out	(9Ah),a		; 12/13
	ld	a,e		; 5
	out	(9Ah),a		; 12/13  COLB takes effect
	inc	hl		; 28
	inc	hl
	inc	hl
	inc	hl
	ld	a,4		; 8      (PALIDX)
.i2	equ	$ - 2
	out	(99h),a		; 12/13
	ld	a,90h		; 8
	out	(99h),a		; 12/13
	ld	a,iyh		; 10
	out	(9Ah),a		; 12/13
	ld	a,iyl		; 10
	out	(9Ah),a		; 12/13  COLA takes effect
.pad:	nop			; 20 or 28, see vdpw
	nop
	nop
	nop
	djnz	plines		; 14   total 74 + 28 + 84 + 28 + 14 = 228
				;      (with VDPW 78 + 28 + 88 + 20 + 14)
	jp	lines.end

	IFNDEF SWEEP
	include	"yjk.inc"
	ELSE
	IF SWEEP = 1
	include	"yjkl.inc"
	ELSE
	IF SWEEP = 2
	include	"yjkm.inc"
	ELSE
	include	"yjkn.inc"
	ENDIF
	ENDIF
	ENDIF

stubs:	dw	stub0, stub1, stub2, stub3, stub4
cnt:	db	0
table:	ds	4 * (K + 1)
end:
