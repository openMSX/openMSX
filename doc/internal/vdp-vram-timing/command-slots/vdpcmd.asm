; VDPCMD (MSX2 and up, V9938/V9958): when a command started at a known
; moment relative to HR has finished. Each trial, as in VDPVRAM:
; - set up a 1-byte HMMV (2 dots in SCREEN 5) to (0,300), wait until CE = 0,
;   send the first byte of the R#46 write (C0h),
; - sync on HR's falling edge (S#2 bit 5), see below,
; - measure the jitter: sample HR K times, one sample per line, each 229
;   cycles after the previous one; CNT = number of samples before HR falls,
; - wait COARSE plus d (0..NPOS-1, 1-cycle steps), then OUT (99h),A with
;   80h+46: the command starts; then S#2 is read five times, 25 cycles
;   apart, the first read 34 (GAP 0) or 46 (GAP 1) cycles after the start
;   of the OUT,
; - count it at e = d + K - CNT in class k (0..4): the first read that saw
;   CE = 0, or 5: none did.
; Three trials per frame: the first waits for VR's falling edge, each one
; then syncs on HR's falling edge (wait for HR = 1, then HR = 0) before its
; HR samples. Memory at CNTA: 6 arrays of NE 16-bit counters (class 0..5),
; then the 16-bit count of invalid trials (BSAVEd by VDPCMD.BAS).
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
NPOS	equ	320		; d = 0..319 = 5*q + r
K	equ	48		; HR samples
NE	equ	NPOS + K	; e = 0..NE-1
NCLASS	equ	6
CNTA	equ	0A000h		; NCLASS arrays of NE 16-bit counters
INVAL	equ	CNTA + 2 * NE * NCLASS	; 16-bit count of invalid trials
NHIST	equ	INVAL + 2	; number of entries in HIST
HIST	equ	NHIST + 1	; up to 16 entries: 3 bytes read back, 16-bit count
HISTMAX	equ	16

	output	"VDPCMD.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	12		; C003h: repetitions per d
coarse:	dw	100		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
gap:	db	0		; C008h: 0..1, the delay of the first read
oldcpu:	db	0

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
	ld	a,(gap)		; the write pair with this padding
	add	a,a
	ld	e,a
	ld	d,0
	ld	hl,writes
	add	hl,de
	ld	a,(hl)
	inc	hl
	ld	h,(hl)
	ld	l,a
	ld	(common+1),hl
	di
	ld	hl,CNTA		; clear the counters and the list
	ld	de,CNTA + 1
	ld	bc,2 * NE * NCLASS + 2 + 1 + 5 * HISTMAX - 1
	ld	(hl),0
	ldir
	xor	a
	ld	(tinf),a
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
	call	trial		; A = class 0..5
	ld	hl,CNTA		; HL = CNTA + A * 2 * NE
	ld	de,2 * NE
	or	a
	jr	z,.cls
	ld	b,a
.ca:	add	hl,de
	djnz	.ca
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
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; one trial; HL = sled entry, IX = stub; returns A, sets (cnt)
trial:	push	hl
	call	cmdset		; (selects S#2)
	pop	hl
	ld	a,(tinf)	; the first trial of a frame: wait for the display
	or	a
	jr	nz,.h1
.w1:	in	a,(99h)		; wait until VR = 1 (vertical blanking)
	and	40h
	jr	z,.w1
.w0:	in	a,(99h)		; wait until VR = 0 (display starts)
	and	40h
	jr	nz,.w0
.h1:	inc	a		; next trial index, 0..2
	cp	3
	jr	c,.ti
	xor	a
.ti:	ld	(tinf),a
.h2:	in	a,(99h)		; wait until HR = 1
	and	20h
	jr	z,.h2
.h3:	in	a,(99h)		; wait until HR = 0: 33 cycles per loop
	and	20h
	jr	nz,.h3
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
	ld	a,0C0h		; first byte of the R#46 write: HMMV
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
common:	jp	cm0		; patched: the start and read for GAP

; the start: OUT (99h),A (12 cycles), padding, IN A,(99h)
cm0:	ld	a,80h + 46
	out	(99h),a
	inc	hl		; 7 + 5 = 12
	nop
	jp	polls		; 10: the first IN 34 cycles after the OUT
cm1:	ld	a,80h + 46
	out	(99h),a
	inc	hl		; 7 + 7 + 5 + 5 + 10 = 34
	inc	hl
	nop
	nop
	neg
	jp	polls		; 10: 46
writes:	dw	cm0, cm1

; five reads of S#2, 25 cycles apart: the class is the first that sees
; CE = 0, 5 if none does
polls:	in	a,(99h)	; 12
	rra			; 5
	jr	nc,.c0		; 8
	in	a,(99h)
	rra
	jr	nc,.c1
	in	a,(99h)
	rra
	jr	nc,.c2
	in	a,(99h)
	rra
	jr	nc,.c3
	in	a,(99h)
	rra
	jr	nc,.c4
	ld	a,5
	jr	rdback
.c0:	xor	a
	jr	rdback
.c1:	ld	a,1
	jr	rdback
.c2:	ld	a,2
	jr	rdback
.c3:	ld	a,3
	jr	rdback
.c4:	ld	a,4

rdback:	ld	(rdbk),a
	call	cewait
	ld	a,(rdbk)	; class 0 to 5
	ret

rdbk:	ds	1

; wait until CE = 0 (S#2 is selected)
cewait:	in	a,(99h)
	rra
	jr	c,cewait
	ret

vwait:	ld	b,10		; ~140 cycles
.w:	djnz	.w
	ret

; the HMMV's registers R#36-R#45 (through R#17), then wait for CE = 0
cmdset:	ld	a,36		; R#17 = 36, auto-increment
	out	(99h),a
	ld	a,80h + 17
	out	(99h),a
	ld	hl,cmdreg
	ld	bc,10 * 256 + 9Bh
	otir
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	jp	cewait
cmdreg:	dw	0, 300, 2, 1	; DX, DY, NX, NY: R#36-R#43
	db	0F0h, 0		; CLR, ARG: R#44, R#45

stubs:	dw	stub0, stub1, stub2, stub3, stub4
repcnt:	db	0
qv:	db	0
rv:	db	0
cnt:	db	0
tinf:	db	0
dval:	dw	0
end:
