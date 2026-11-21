; VDP edge timing (MSX2 and up, V9938/V9958): where do VR, HR and the VRAM
; access slots lie relative to the falling edge of HR?
; MODE 0: VR rise (end of the display), MODE 1: VR fall one frame later,
; MODE 2: HR (S#2 bit 5, both edges are in the range), MODE 3: two quick VRAM
; writes (OUT (C),A / OUT (C),B): is the second one dropped?
; MODE 4: the same writes as MODE 3, but records what was read back from
; 7000h-7002h: (AAh,55h,00h) -> "one", (55h,00h,00h) -> "zero", anything
; else -> "other"; plus a list of all triples read back, with their counts.
; Each trial (one frame):
; - sync on the falling edge of VR (S#2 bit 6),
; - measure the jitter: sample HR K times, one sample per line, each 229
;   cycles after the previous one; CNT = number of samples before HR falls,
; - wait COARSE plus d (0..NPOS-1, 1-cycle steps), then
;   MODE 0-2: read S#2 once: bit set -> "one", clear -> "zero";
;   MODE 3: write AAh and 55h to VRAM 7000h, read them back later:
;   both there -> "one", second missing -> "zero", anything else -> "other",
; - count the outcome at e = d + K - CNT.
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
NHIST	equ	INVAL + 2	; MODE 4: number of entries in HIST
HIST	equ	NHIST + 1	; MODE 4: up to 16 entries: 3 bytes read back, 16-bit count
HISTMAX	equ	16

	output	"VDPEDGE.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	16		; C003h: repetitions per d
coarse:	dw	1075		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
mode:	db	0		; C008h: 0 = VR rise, 1 = VR fall, 2 = HR, 3/4 = VRAM writes
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
	ld	hl,cstat	; the measurement after the delay
	ld	a,(mode)
	cp	3
	jr	c,.st
	ld	hl,cvram
	jr	z,.st
	ld	hl,cvram4
.st:	ld	(common+1),hl
	di
	ld	hl,LOSTA	; clear the counters
	ld	de,LOSTA + 1
	ld	bc,6 * NE + 2 + 1 + 5 * HISTMAX - 1
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
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; one trial; HL = sled entry, IX = stub; returns A, sets (cnt)
trial:	ld	a,(mode)
	cp	3
	call	nc,vclear
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
	ld	a,(mode)
	cp	3
	call	nc,vwaddr	; MODE 3/4: VRAM write address 7000h
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
common:	jp	cstat		; patched: cstat or cvram

cstat:	in	a,(99h)		; S#2
	ld	e,a
	ld	a,(mask)
	and	e
	ret	z		; 0: zero
	ld	a,1		; 1: one
	ret

cvram:	ld	bc,5598h	; 11
	ld	a,0AAh		; 8
	out	(c),a		; the first write
	out	(c),b		; the second write, 14 cycles later
	ld	b,0
.dl:	djnz	.dl		; ~3600 cycles
	xor	a		; read address 7000h (R#14 is still 1)
	out	(99h),a
	ld	a,30h
	out	(99h),a
	call	vwait
	in	a,(98h)
	ld	c,a
	call	vwait
	in	a,(98h)
	ld	b,a
	ld	a,c
	cp	0AAh
	jr	nz,.oth
	ld	a,b
	cp	55h
	ld	a,1
	ret	z		; 1: both written
	ld	a,b
	or	a
	ld	a,2
	ret	z		; 2: the second one is missing
.oth:	xor	a		; 0: anything else
	ret

cvram4:	ld	bc,5598h	; the same as cvram up to the reads
	ld	a,0AAh
	out	(c),a
	out	(c),b
	ld	b,0
.dl:	djnz	.dl
	xor	a
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
.cls:	ld	hl,rdbk		; classify
	ld	a,(hl)
	inc	hl
	cp	0AAh
	jr	nz,.c2
	ld	a,(hl)
	cp	55h
	jr	nz,.oth
	inc	hl
	ld	a,(hl)
	or	a
	jr	nz,.oth
	ld	a,1
	ret			; 1: (AA,55,00)
.c2:	cp	55h
	jr	nz,.oth
	ld	a,(hl)
	inc	hl
	or	(hl)
	jr	nz,.oth
	ld	a,2
	ret			; 2: (55,00,00)
.oth:	xor	a
	ret

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
masks:	db	40h, 40h, 20h, 0, 0
repcnt:	db	0
qv:	db	0
rv:	db	0
cnt:	db	0
dval:	dw	0
end:
