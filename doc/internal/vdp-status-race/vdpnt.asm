; VDPNT (MSX2 and up): how long is a field when NT (R#9 bit 1, 50/60 Hz)
; changes in it? Per call: R#9 = NTA, settle 3 frames; sync on VR's fall
; (the display start), count N lines (HR pulses), write R#9 = NTB, then
; count the lines to VR's next four edges (rise, fall, rise, fall). The
; counts are a line from the write to the edge, to within one line. DLY
; adds a delay of 14 Z80 cycles per step between the Nth line and the
; write (0: none).
; Interrupts are off during the call.

MSXVER	equ	002Dh
CHGCPU	equ	0180h
GETCPU	equ	0183h

	output	"VDPNT.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
nlines:	dw	0		; C003h: N, lines after the display start
nta:	db	82h		; C005h: R#9 before
ntb:	db	80h		; C006h: R#9 written after N lines
res:	ds	8		; C007h: four 16-bit counts
oldcpu:	db	0		; C00Fh
dly:	db	0		; C010h: 14 Z80 cycles per step before the write

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode
	call	CHGCPU
.notr:	di
	ld	a,(nta)		; R#9 = NTA
	out	(99h),a
	ld	a,89h
	out	(99h),a
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	b,3		; let 3 frames pass
.st:	push	bc
	call	vr1
	call	vr0
	pop	bc
	djnz	.st
	call	vr1		; sync: VR falls = the display starts
	call	vr0
	ld	hl,(nlines)	; count N lines
.nl:	ld	a,h
	or	l
	jr	z,.wr
	call	line
	dec	hl
	jr	.nl
.wr:	ld	a,(dly)		; the delay within the line
	or	a
	jr	z,.w2
	ld	b,a
.dl:	djnz	.dl		; 14 cycles per step
.w2:	ld	a,(ntb)		; write R#9 = NTB
	out	(99h),a
	ld	a,89h
	out	(99h),a
	ld	ix,res
	ld	c,40h		; to VR's rise
	call	count
	ld	c,0		; to VR's fall
	call	count
	ld	c,40h
	call	count
	ld	c,0
	call	count
	xor	a		; S#0 again
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

; wait for VR = 1, VR = 0
vr1:	in	a,(99h)
	and	40h
	jr	z,vr1
	ret
vr0:	in	a,(99h)
	and	40h
	jr	nz,vr0
	ret

; one line: wait for HR = 1, then HR = 0
line:	in	a,(99h)
	and	20h
	jr	z,line
.h:	in	a,(99h)
	and	20h
	jr	nz,.h
	ret

; count lines until VR == C (40h or 0); store the 16-bit count at IX
count:	ld	de,0
.l:	in	a,(99h)
	ld	b,a
	and	40h
	cp	c
	jr	z,.done
	bit	5,b
	jr	z,.l		; HR = 0: keep looking
.h:	in	a,(99h)		; HR = 1: wait for its fall, one line more
	and	20h
	jr	nz,.h
	inc	de
	jr	.l
.done:	ld	(ix+0),e
	ld	(ix+1),d
	inc	ix
	inc	ix
	ret
end:
