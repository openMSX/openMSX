; VDPLINE: shows on screen where a write timed relative to HR lands in the
; picture (MSX2 and up, V9938/V9958).
; Each frame: sync on the falling edge of VR, measure the phase with K HR
; samples (as in vdprace/vdpmode: CNT = number of samples before HR falls),
; then wait a fixed time plus CNT cycles, so that what follows always starts
; at the same moment relative to HR. Then, for NLINES lines, exactly 228 cycles
; per line: write COLB to R#7, and 114 cycles later COLA. In SCREEN 5 that
; colours the backdrop (colour 0 pixels and the border), in text mode the
; text background: a vertical stripe whose edges show where the two writes
; land in the line. If the loop isn't exactly one line long, the edges slant.
; Interrupts are off; space (checked once per frame) stops it.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle); VDPW = 1 adds
; 1 cycle to every VDP I/O (T9769, S1990), which the line loop compensates.

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
K	equ	48		; HR samples

	output	"VDPLIN1.BIN"
	org	0C000h - 7
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
	di
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
	ld	a,(colb)
	ld	d,a
	ld	e,80h + 7
	ld	a,(cola)
	jp	(hl)		; 5
	ds	10,0		; NOP sled: 5 cycles per NOP
sledend:jp	(ix)		; 10
stub0:	inc	hl		; 14
	inc	hl
	jp	lines
stub1:	nop			; 15
	nop
	nop
	jp	lines
stub2:	nop			; 5 + 11 = 16
	jp	.s2
.s2:	jp	lines
stub3:	nop			; 17
	nop
	inc	hl
	jp	lines
stub4:	inc	hl		; 7 + 11 = 18
	jp	.s4
.s4:	jp	lines

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
	in	a,(0AAh)	; space bar: keyboard row 8, bit 0
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jp	c,frame
	xor	a		; select S#0 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

stubs:	dw	stub0, stub1, stub2, stub3, stub4
cnt:	db	0
table:	ds	4 * (K + 1)
end:
