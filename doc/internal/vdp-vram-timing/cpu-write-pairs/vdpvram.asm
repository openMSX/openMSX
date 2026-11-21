; VDPVRAM (MSX2 and up, V9938/V9958): two VRAM writes in quick succession,
; at a known moment relative to HR, and what ends up in VRAM.
; Each trial (one frame), as in VDPEDGE:
; - sync on the falling edge of VR (S#2 bit 6),
; - measure the jitter: sample HR K times, one sample per line, each 229
;   cycles after the previous one; CNT = number of samples before HR falls,
; - wait COARSE plus d (0..NPOS-1, 1-cycle steps), then write AAh and 55h
;   to VRAM 7000h with OUT (C),A / OUT (C),B, GAP selecting the padding in
;   between: 0, 5, 7, 10, 14, 21 or 28 cycles (so the second OUT starts 14,
;   19, 21, 24, 28, 35 or 42 cycles after the first; 1 more with VDPW),
; - read back 7000h-7002h (cleared to 0 before the trial) and count it at
;   e = d + K - CNT in one of six classes: 0 = (AAh, 55h, 00h), 1 = (55h,
;   00h, 00h), 2 = (55h, 55h, 00h), 3 = (AAh, AAh, 00h), 4 = (00h, 55h,
;   00h), 5 = anything else; plus a list of all triples with their counts.
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

	output	"VDPVRAM.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	12		; C003h: repetitions per d
coarse:	dw	100		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
gap:	db	0		; C008h: 0..6, the padding between the two writes
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
trial:	call	vclear
	ld	a,2		; select S#2
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
	call	vwaddr		; VRAM write address 7000h
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
common:	jp	wr0		; patched: the write pair for GAP

; the write pairs: LD BC (11), LD A (8), OUT (C),A, padding, OUT (C),B
wr0:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	out	(c),b		; 14 cycles after the first
	jp	rdback
wr1:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	nop			; 5
	out	(c),b		; 19
	jp	rdback
wr2:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	inc	hl		; 7
	out	(c),b		; 21
	jp	rdback
wr3:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	nop			; 10
	nop
	out	(c),b		; 24
	jp	rdback
wr4:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	inc	hl		; 14
	inc	hl
	out	(c),b		; 28
	jp	rdback
wr5:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	inc	hl		; 21
	inc	hl
	inc	hl
	out	(c),b		; 35
	jp	rdback
wr6:	ld	bc,5598h
	ld	a,0AAh
	out	(c),a
	inc	hl		; 28
	inc	hl
	inc	hl
	inc	hl
	out	(c),b		; 42
	jp	rdback
writes:	dw	wr0, wr1, wr2, wr3, wr4, wr5, wr6

rdback:	ld	b,0
.dl:	djnz	.dl		; ~3600 cycles
	xor	a		; read address 7000h (R#14 is still 1)
	out	(99h),a
	ld	a,30h
	out	(99h),a
	call	vwait
	in	a,(98h)
	ld	(rdbk),a
	call	vwait
	in	a,(98h)
	ld	(rdbk+1),a
	call	vwait
	in	a,(98h)
	ld	(rdbk+2),a
	ld	hl,HIST		; count this triple in HIST
	ld	a,(NHIST)
	or	a
	jr	z,.new
	ld	b,a
.find:	ld	de,rdbk
	push	hl
	ld	c,3
.cmp:	ld	a,(de)
	cp	(hl)
	jr	nz,.ne
	inc	de
	inc	hl
	dec	c
	jr	nz,.cmp
	inc	(hl)		; found: count + 1
	jr	nz,.f1
	inc	hl
	inc	(hl)
.f1:	pop	hl
	jr	.cls
.ne:	pop	hl
	ld	de,5
	add	hl,de
	djnz	.find
	ld	a,(NHIST)
	cp	HISTMAX
	jr	nc,.cls		; full: not listed
.new:	ex	de,hl
	ld	hl,rdbk
	ld	bc,3
	ldir
	ex	de,hl
	ld	(hl),1
	ld	hl,NHIST
	inc	(hl)
.cls:	ld	a,(rdbk+2)	; classify: the third byte must be 0
	or	a
	ld	a,5
	ret	nz
	ld	hl,classes
	ld	b,5
.c1:	ld	a,(rdbk)
	cp	(hl)
	jr	nz,.c2
	inc	hl
	ld	a,(rdbk+1)
	cp	(hl)
	inc	hl
	jr	nz,.c3
	ld	a,(hl)
	ret
.c2:	inc	hl
	inc	hl
.c3:	inc	hl
	djnz	.c1
	ld	a,5
	ret
classes:db	0AAh, 55h, 0
	db	55h, 00h, 1
	db	55h, 55h, 2
	db	0AAh, 0AAh, 3
	db	00h, 55h, 4

rdbk:	ds	3

vwait:	ld	b,10		; ~140 cycles
.w:	djnz	.w
	ret

vwaddr:	ld	a,1		; R#14 = 1, write address 7000h
	out	(99h),a
	ld	a,80h + 14
	out	(99h),a
	xor	a
	out	(99h),a
	ld	a,70h
	out	(99h),a
	ret

vclear:	call	vwaddr		; VRAM 7000h-7002h = 0
	call	vwait
	xor	a
	out	(98h),a
	call	vwait
	xor	a
	out	(98h),a
	call	vwait
	xor	a
	out	(98h),a
	jp	vwait

stubs:	dw	stub0, stub1, stub2, stub3, stub4
repcnt:	db	0
qv:	db	0
rv:	db	0
cnt:	db	0
dval:	dw	0
end:
