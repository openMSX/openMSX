; Do the CPU clock and the VDP clock run at exactly 1:6?
; Per trial (interrupts off): sync on the falling edge of VR, measure the phase
; of HR in the line (C1: 48 samples, 229 cycles apart, count of HR=1), wait so
; that the second measurement starts exactly 313 lines (313 * 228 = 71364 CPU
; cycles) after the first one, measure again (C2). With locked clocks C2 = C1;
; each cycle of difference is 1/71364 = 14 ppm.

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
K	equ	64

	output	"CLKTEST.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	64		; C003h: trials (results at C200h: C1, C2 per trial)
coarse:	dw	1884		; C004h: coarse delay loop count (30 cycles each)
dfine:	db	3		; C006h: fine delay 0..255 cycles
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle
hrwait:	db	20		; C008h: delay before the first samples (18 cycles each)
oldcpu:	db	0
sledp:	dw	0
stubp:	dw	0
resp:	dw	0

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode
	call	CHGCPU
.notr:	ld	a,(vdpw)	; without the extra wait, IN is 1 cycle shorter
	or	a
	ld	a,44h		; NEG: 10 cycles
	jr	nz,.w
	ld	a,57h		; LD A,I: 11 cycles
.w:	ld	(sample.fix+1),a
	ld	a,(dfine)	; q = dfine / 5, r = dfine % 5
	ld	b,0
.div:	cp	5
	jr	c,.divd
	sub	5
	inc	b
	jr	.div
.divd:	add	a,a		; stub = stubs[r]
	ld	e,a
	ld	d,0
	ld	hl,stubs
	add	hl,de
	ld	e,(hl)
	inc	hl
	ld	d,(hl)
	ld	(stubp),de
	ld	e,b		; sled entry = sledend - q
	ld	d,0
	ld	hl,sledend
	or	a
	sbc	hl,de
	ld	(sledp),hl
	ld	hl,0C200h
	ld	(resp),hl
	di
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(reps)
	ld	b,a
.trial:	push	bc
.w1:	in	a,(99h)		; wait until VR = 1
	and	40h
	jr	z,.w1
.w0:	in	a,(99h)		; wait until VR = 0
	and	40h
	jr	nz,.w0
	ld	a,(hrwait)
	ld	e,a
.hw:	dec	e		; 18 cycles per loop
	jr	nz,.hw
	call	sample		; C1
	ld	hl,(resp)
	ld	(hl),a
	inc	hl
	ld	(resp),hl
	ld	hl,(sledp)
	ld	ix,(stubp)
	ld	bc,(coarse)
.cd:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.cd
	jp	(hl)
	ds	51,0		; NOP sled: 5 cycles per NOP (q = 0..51)
sledend:jp	(ix)
stub0:	inc	hl		; 14
	inc	hl
	jp	back
stub1:	nop			; 15
	nop
	nop
	jp	back
stub2:	nop			; 16
	ld	a,i
	jp	back
stub3:	inc	hl		; 17
	neg
	jp	back
stub4:	inc	hl		; 18
	ld	a,i
	jp	back
back:	call	sample		; C2
	ld	hl,(resp)
	ld	(hl),a
	inc	hl
	ld	(resp),hl
	pop	bc
	dec	b
	jp	nz,main.trial
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

; 64 samples of HR, one every 229 cycles; returns the count of HR=1 in A
sample:	ld	bc,K * 256
.smp:	in	a,(99h)
	rlca			; bit 5 -> bit 0
	rlca
	rlca
	and	1
	add	a,c
	ld	c,a
	ld	e,8		; padding: 8 + 7*18 + 13 + 5 + 7 + 10 = 169
.pad:	dec	e
	jr	nz,.pad
	nop
	inc	de
.fix:	neg			; (or LD A,I, see vdpw)
	djnz	.smp
	ld	a,c
	ret

stubs:	dw	stub0, stub1, stub2, stub3, stub4
end:
