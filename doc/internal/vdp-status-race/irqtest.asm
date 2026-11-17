; Does a read of S#0 at the moment F is set also lose the VDP interrupt?
; (turboR only: uses the E6 timer as a reference for the number of frames.)
; Interrupts on, IM2 handler counts the interrupts (and how often its own read
; of S#0 still sees F); the main loop keeps polling S#0 (and counts how often it
; sees F itself) until the E6 timer has wrapped WRAPS times (65536 ticks of 14
; Z80 cycles each). Frames = ticks * 14 / 59736 (60 Hz) or / 71364 (50 Hz).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh

	output	"IRQTEST.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
wraps:	db	40		; C003h: E6 timer wraps (0.256 s each)
oldcpu:	db	0
	ds	0C010h - $, 0
isrcnt:	dw	0		; C010h: interrupts
isrf:	dw	0		; C012h: interrupts whose read of S#0 saw F
mainf:	dw	0		; C014h: F seen by the main loop
rest:	dw	0		; C016h: E6 ticks after the last wrap

main:	ld	a,(MSXVER)
	cp	3
	ret	c		; not a turboR
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode
	call	CHGCPU
	di
	ld	hl,0C100h	; IM2 table: all vectors point to C2C2h
	ld	de,0C101h
	ld	bc,256
	ld	(hl),0C2h
	ldir
	ld	hl,0
	ld	(isrcnt),hl
	ld	(isrf),hl
	ld	(mainf),hl
	ld	a,0C1h
	ld	i,a
	im	2
	in	a,(99h)		; clear F (R#15 is 0 in BASIC)
	out	(0E6h),a	; reset the timer
	ld	a,(wraps)
	ld	d,a
	ld	e,0		; previous high byte of the timer
	ei
.loop:	in	a,(99h)		; poll S#0
	and	a
	jp	p,.nof
	ld	hl,(mainf)
	inc	hl
	ld	(mainf),hl
.nof:	in	a,(0E7h)	; timer high byte
	cp	e
	ld	e,a
	jr	nc,.loop	; no wrap
	dec	d
	jr	nz,.loop
	di
	in	a,(0E6h)
	ld	l,a
	in	a,(0E7h)
	ld	h,a
	ld	(rest),hl
	im	1
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
	ei
	ret

	ds	0C2C2h - $, 0
handler:
	push	af
	push	hl
	ld	hl,(isrcnt)
	inc	hl
	ld	(isrcnt),hl
	in	a,(99h)		; S#0: acknowledges the interrupt
	and	a
	jp	p,.h1
	ld	hl,(isrf)
	inc	hl
	ld	(isrf),hl
.h1:	pop	hl
	pop	af
	ei
	reti
end:
