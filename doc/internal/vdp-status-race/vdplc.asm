; VDPLC: when does the VDP take LN (R#9 bit 7, 192/212 lines) for the next
; frame, i.e. where in its line is the line-count reset? (MSX2 and up,
; V9938/V9958; based on VDPMODE.)
; Screen mode A (MA0/MA1) during the sync and the HR samples, mode B
; (MB0/MB1) from then on, R#18 and R#25 as in VDPMODE. LN is 1 (212 lines)
; except for the write below.
; Each trial (two frames):
; - sync on the falling edge of VR (the display start of frame N) and measure
;   the phase with K HR samples (CNT), as VDPMODE,
; - wait COARSE plus d (0..NPOS-1, 1-cycle steps) into the line-count reset
;   line of frame N+1 and write R#9 with LN = 0 there (the value byte is
;   written before the wait, the register byte at the measured moment),
; - wait COARSE2 and read S#2 about 5 lines after the display start of
;   frame N+1 with 212 lines: the reset fixes the display start from LN as it
;   is then, and with 192 lines the display starts 10 lines later (PAL), so VR
;   is still 1 there (the display end compares the line counter with LN as it
;   is at that moment, so it can't show when LN was taken),
; - write R#9 with LN = 1 again (before the reset of frame N+2):
;     VR=1 -> "seen" (the write came before the reset), VR=0 -> "before"
;   (it came after it),
; - count the outcome at e = d + K - CNT, as VDPMODE.
; Trials with CNT = 0 or K are only counted. COARSE and COARSE2 depend on
; the frame (50 or 60 Hz): see autoexec-vdplc.bas.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG0SAV	equ	0F3DFh
RG1SAV	equ	0F3E0h
RG8SAV	equ	0FFE7h
RG9SAV	equ	0FFE8h
RG18SAV	equ	0FFF1h
RG19SAV	equ	0FFF2h
RG25SAV	equ	0FFFAh
NPOS	equ	320		; d = 0..319 = 5*q + r
K	equ	48		; HR samples
NE	equ	NPOS + K	; e = 0..NE-1
LOSTA	equ	0C800h		; 16-bit counters per e
SEENA	equ	LOSTA + 2 * NE
BEFA	equ	SEENA + 2 * NE
INVAL	equ	BEFA + 2 * NE	; 16-bit count of invalid trials

	output	"VDPLC.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	16		; C003h: repetitions per d
coarse:	dw	1604		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
mode:	db	2		; C008h: unused (S#2)
line:	db	0		; C009h: unused
ma0:	db	04h		; C00Ah: screen mode A, M bits of R#0 (TEXT2: 04h, GRAPHIC4: 06h)
ma1:	db	10h		; C00Bh: screen mode A, M bits of R#1 (TEXT2: 10h, GRAPHIC4: 00h)
mb0:	db	04h		; C00Ch: screen mode B, M bits of R#0
mb1:	db	10h		; C00Dh: screen mode B, M bits of R#1
r18:	db	0FFh		; C00Eh: R#18 during the test (FFh: unchanged)
r25:	db	0FFh		; C00Fh: R#25 during the test (FFh: unchanged)
coarse2:dw	422		; C010h: delay from the write to the S#2 read (30 cycles each)
r9hi:	db	0		; R#9 with LN = 1 and LN = 0
r9lo:	db	0
ra0:	db	0		; R#0 and R#1 values for mode A and B
ra1:	db	0
rb0:	db	0
rb1:	db	0
oldcpu:	db	0
mask:	db	80h

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode, update the LED
	call	CHGCPU
.notr:	ld	a,(vdpw)	; without the extra wait, IN is 1 cycle shorter:
	or	a		; make the sample loop 1 cycle longer
	ld	a,44h		; NEG: 10 cycles
	jr	nz,.w
	ld	a,57h		; LD A,I: 11 cycles
.w:	ld	(trial.fix+1),a
	ld	a,(mode)	; the bit to test
	ld	hl,masks
	ld	e,a
	ld	d,0
	add	hl,de
	ld	a,(hl)
	ld	(mask),a
	ld	a,(mode)	; the status register to read
	ld	hl,sregs
	ld	e,a
	ld	d,0
	add	hl,de
	ld	a,(hl)
	ld	(sreg),a
	ld	a,(RG0SAV)	; R#0: M3-M5 from MA0/MB0, IE1 in MODE 1
	and	0F1h
	ld	b,a
	ld	a,(mode)
	cp	1
	jr	nz,.noie
	set	4,b
.noie:	ld	a,(ma0)
	or	b
	ld	(ra0),a
	ld	a,(mb0)
	or	b
	ld	(rb0),a
	ld	a,(RG1SAV)	; R#1: M1, M2 from MA1/MB1
	and	0E7h
	ld	b,a
	ld	a,(ma1)
	or	b
	ld	(ra1),a
	ld	a,(mb1)
	or	b
	ld	(rb1),a
	di
	ld	a,(RG8SAV)	; sprites off
	or	02h
	out	(99h),a
	ld	a,80h + 8
	out	(99h),a
	ld	a,(r18)
	cp	0FFh
	jr	z,.no18
	out	(99h),a
	ld	a,80h + 18
	out	(99h),a
.no18:	ld	a,(r25)
	cp	0FFh
	jr	z,.no25
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
.no25:	ld	a,(RG9SAV)	; R#9: LN = 1 during the test
	or	80h
	ld	(r9hi),a
	out	(99h),a
	ld	a,80h + 9
	out	(99h),a
	ld	a,(RG9SAV)
	and	7Fh
	ld	(r9lo),a
	ld	a,(mode)
	cp	1
	jr	nz,.nofh
	ld	a,(line)	; MODE 1: R#19 = LINE
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
.nofh:
	ld	hl,LOSTA	; clear the counters
	ld	de,LOSTA + 1
	ld	bc,6 * NE + 2 - 1
	ld	(hl),0
	ldir
	ld	a,(reps)
	ld	(repcnt),a
.rep:	xor	a
	ld	(qv),a
	ld	(rv),a
	ld	hl,0
	ld	(dval),hl
.pos:	ld	a,(qv)		; HL = sled entry, IX = stub
	ld	e,a
	ld	d,0
	ld	hl,sledend
	or	a
	sbc	hl,de
	push	hl
	ld	a,(rv)
	add	a,a
	ld	e,a
	ld	ix,stubs
	add	ix,de
	ld	a,(ix+0)
	ld	h,(ix+1)
	ld	l,a
	push	hl
	pop	ix
	pop	hl
	call	trial		; A = 0 before, 1 seen, 2 lost
	ld	hl,BEFA
	or	a
	jr	z,.cls
	ld	hl,SEENA
	dec	a
	jr	z,.cls
	ld	hl,LOSTA
.cls:	ld	a,(cnt)
	or	a
	jr	z,.inv
	cp	K
	jr	z,.inv
	neg			; HL += 2 * (d + K - cnt)
	add	a,K
	ld	de,(dval)
	ex	de,hl
	ld	c,a
	ld	b,0
	add	hl,bc
	add	hl,hl
	add	hl,de
	jr	.inc
.inv:	ld	hl,INVAL
.inc:	inc	(hl)
	jr	nz,.next
	inc	hl
	inc	(hl)
.next:	ld	de,(dval)
	inc	de
	ld	(dval),de
	ld	a,(rv)
	inc	a
	cp	5
	jr	c,.nor
	ld	a,(qv)
	inc	a
	ld	(qv),a
	xor	a
.nor:	ld	(rv),a
	ld	hl,NPOS
	or	a
	sbc	hl,de
	jp	nz,.pos
	ld	hl,repcnt
	dec	(hl)
	jp	nz,.rep
	xor	a		; select S#0 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(RG0SAV)	; restore R#0, R#1, R#8, R#18
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(RG1SAV)
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,(RG8SAV)
	out	(99h),a
	ld	a,80h + 8
	out	(99h),a
	ld	a,(RG18SAV)
	out	(99h),a
	ld	a,80h + 18
	out	(99h),a
	ld	a,(RG9SAV)	; and R#9
	out	(99h),a
	ld	a,80h + 9
	out	(99h),a
	ld	a,(r25)		; restore R#25 if it was changed
	cp	0FFh
	jr	z,.nor25
	ld	a,(RG25SAV)
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
.nor25:
	ld	a,(mode)
	cp	1
	jr	nz,.nofh2
	ld	a,(RG19SAV)	; MODE 1: restore R#19
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
	ld	a,1		; clear FH
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)
	xor	a
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
.nofh2:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; one trial; HL = sled entry, IX = stub; returns A, sets (cnt)
trial:	ld	a,(ra0)		; screen mode A
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(ra1)
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
.w2:	in	a,(99h)		; wait until VR = 0 (the previous trial may end
	and	40h		; before a delayed display start)
	jr	nz,.w2
.w1:	in	a,(99h)		; wait until VR = 1 (vertical blanking)
	and	40h
	jr	z,.w1
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
	djnz	.smp		; 14   total 229 (measured in openMSX)
	ld	a,c
	ld	(cnt),a
	ld	a,(rb0)		; screen mode B
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(rb1)
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,(sreg)	; select the status register
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(r9lo)	; the value byte of the R#9 write
	out	(99h),a
	ld	bc,(coarse)
.cd:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.cd
	jp	(hl)		; 5 cycles
	ds	63,0		; NOP sled: 5 cycles per NOP
sledend:jp	(ix)		; 10 cycles
stub0:	inc	hl		; 7+7 = 14
	inc	hl
	jp	common
stub1:	nop			; 5+5+5 = 15
	nop
	nop
	jp	common
stub2:	nop			; 5+11 = 16
	ld	a,i
	jp	common
stub3:	inc	hl		; 7+10 = 17
	neg
	jp	common
stub4:	inc	hl		; 7+11 = 18
	ld	a,i
	jp	common
common:	ld	a,80h + 9	; 8
	out	(99h),a		; the measured write: R#9 with LN = 0
	ld	bc,(coarse2)
.c2:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.c2
	in	a,(99h)		; S#2 about 5 lines into the display of frame N+1
	ld	c,a
	ld	a,(r9hi)	; LN = 1 again
	out	(99h),a
	ld	a,80h + 9
	out	(99h),a
	ld	a,c
	and	40h
	ret	z		; VR = 0: "before" (LN = 1 taken: display started)
	ld	a,1
	ret			; VR = 1: "seen" (LN = 0 taken: display starts later)

stubs:	dw	stub0, stub1, stub2, stub3, stub4
masks:	db	80h, 01h, 40h, 20h
sregs:	db	0, 1, 2, 2
sreg:	db	0
repcnt:	db	0
qv:	db	0
rv:	db	0
cnt:	db	0
dval:	dw	0
end:
