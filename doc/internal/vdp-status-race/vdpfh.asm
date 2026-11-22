; VDPFH: shows on screen where a write timed with the line interrupt lands in
; the picture (MSX2 and up, V9938/V9958).
; Only the line interrupt is enabled (IE1, line LINE; IE0 off). The CPU waits
; with HALT in interrupt mode 2, with its own vector table, so the interrupt
; goes straight to ISR, without the BIOS. ISR waits DELAY (a fixed time), then,
; for NLINES lines, exactly 228 cycles per line, writes COLB to R#7 and 114
; cycles later COLA (the same loop as VDPLINE), then clears FH by reading S#1.
; The jitter is that of HALT (a HALT cycle is 5 cycles): constant on a machine
; whose CPU and VDP clocks are locked, otherwise up to 5 cycles.
; Space (checked once per frame) stops it.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle); VDPW = 1 adds
; 1 cycle to every VDP I/O (T9769, S1990), which the line loop compensates.

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG0SAV	equ	0F3DFh
RG1SAV	equ	0F3E0h
RG19SAV	equ	0FFF2h
VTAB	equ	0B000h		; 257 bytes of 0B1h: vector for any data byte
ISRADR	equ	0B1B1h		; where the vector points: JP ISR

	output	"VDPFH.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
delay:	dw	10		; C003h: delay loop count in ISR (30 cycles each)
line:	db	100		; C005h: R#19, the line of the interrupt
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
	di
	ld	hl,VTAB		; the vector table
	ld	de,VTAB + 1
	ld	bc,256
	ld	(hl),0B1h
	ldir
	ld	a,0C3h		; JP ISR at the vector
	ld	(ISRADR),a
	ld	hl,isr
	ld	(ISRADR + 1),hl
	ld	a,VTAB / 256
	ld	i,a
	im	2
.rel:	in	a,(0AAh)	; wait until space is released
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jr	nc,.rel
	ld	a,(line)	; R#19 = LINE
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
	ld	a,(RG1SAV)	; R#1: IE0 off
	and	0DFh
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,(RG0SAV)	; R#0: IE1 on
	or	10h
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,1		; clear FH: read S#1
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)
	xor	a		; S#0
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)		; clear F

frame:	ei
	halt			; -> ISR, which returns with interrupts off
	in	a,(0AAh)	; space bar: keyboard row 8, bit 0
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jr	c,frame
	ld	a,(RG0SAV)	; restore R#0, R#1, R#19
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(RG1SAV)
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,(RG19SAV)
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
	ld	a,(cola)	; and R#7
	out	(99h),a
	ld	a,80h + 7
	out	(99h),a
	im	1
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

isr:	push	af		; constant time from the interrupt on
	push	bc
	push	de
	push	hl
	ld	bc,(delay)
.d:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.d
	ld	a,(nlines)
	ld	b,a
	ld	c,99h
	ld	a,(colb)
	ld	d,a
	ld	e,80h + 7
	ld	a,(cola)
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
	ld	a,1		; clear FH: read S#1
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)
	xor	a		; S#0 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret			; interrupts stay off: FRAME does EI
end:
