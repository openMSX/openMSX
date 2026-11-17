; VDP status flag race measurement for the slot tests (MSX2 and up, V9938/V9958):
; vdprace.asm plus BLANK (display off during the test). The slot test results
; were made with this program.
; MODE 0: S#0 bit 7 (F), MODE 1: S#1 bit 0 (FH; IE1 is switched on and R#19 set
; to LINE, interrupts stay off in the CPU), MODE 2: S#2 bit 6 (VR, which can't
; be lost: a reference for the time of the vertical blanking).
; Each trial (one frame):
; - sync on the falling edge of VR (S#2 bit 6); this has a jitter of the
;   polling loop length,
; - measure that jitter: sample HR (S#2 bit 5) K times, one sample per line,
;   each 229 cycles after the previous one (a line is 228 cycles), so each
;   sample is 1 cycle later in the line; the samples start inside HR=1, so
;   CNT = number of samples before HR falls,
; - wait COARSE plus d (0..NPOS-1, 1-cycle steps), read S#0 once (A1), read
;   it again ~3600 cycles later (A2):
;     A1.7=1          -> seen,  A1.7=0 A2.7=1 -> before,  A1.7=0 A2.7=0 -> lost
; - count the outcome at e = d + K - CNT: the read time relative to the
;   VDP's own timing (in Z80 cycles, plus a constant).
; Trials with CNT = 0 or K (HR edge not inside the samples) are only counted.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG0SAV	equ	0F3DFh
RG1SAV	equ	0F3E0h
RG19SAV	equ	0FFF2h
NPOS	equ	320		; d = 0..319 = 5*q + r
K	equ	48		; HR samples
NE	equ	NPOS + K	; e = 0..NE-1
LOSTA	equ	0C800h		; 16-bit counters per e
SEENA	equ	LOSTA + 2 * NE
BEFA	equ	SEENA + 2 * NE
INVAL	equ	BEFA + 2 * NE	; 16-bit count of invalid trials

	output	"SLOTTEST.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	16		; C003h: repetitions per d
coarse:	dw	1075		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
mode:	db	0		; C008h: 0 = S#0/F, 1 = S#1/FH, 2 = S#2/VR
line:	db	191		; C009h: R#19 for MODE 1
blank:	db	0		; C00Ah: 1 = display off (BL=0 in R#1) during the test
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
	di
	ld	a,(mode)
	cp	1
	jr	nz,.nofh
	ld	a,(line)	; MODE 1: R#19 = LINE, R#0 |= IE1
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
	ld	a,(RG0SAV)
	or	10h
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
.nofh:	ld	a,(blank)
	or	a
	jr	z,.noblank
	ld	a,(RG1SAV)	; BL=0
	and	0BFh
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
.noblank:
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
	ld	a,(mode)
	cp	1
	jr	nz,.nofh2
	ld	a,(RG0SAV)	; MODE 1: restore R#0 and R#19
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(RG19SAV)
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
.nofh2:	ld	a,(RG1SAV)	; restore R#1
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; one trial; HL = sled entry, IX = stub; returns A, sets (cnt)
trial:	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
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
	ld	a,(mode)	; select S#MODE
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)		; clear the flag of the previous frame
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
common:	in	a,(99h)		; A1: the measured read
	ld	c,a
	ld	b,0
.dl:	djnz	.dl		; ~3600 cycles, still in vertical blanking
	in	a,(99h)		; A2
	ld	b,a
	ld	a,(mask)
	ld	e,a
	ld	a,c
	and	e
	ld	a,1
	ret	nz		; seen
	ld	a,b
	and	e
	ld	a,0
	ret	nz		; before
	ld	a,2
	ret			; lost

stubs:	dw	stub0, stub1, stub2, stub3, stub4
masks:	db	80h, 01h, 40h
repcnt:	db	0
qv:	db	0
rv:	db	0
cnt:	db	0
dval:	dw	0
end:
