; VDPIRQH: VDPIRQ for the line interrupt: when does /INT come, relative to
; FH? VDPMODE MODE 1 (FH: IE1 on, R#19 = LINE; IE0 off) with the interrupt
; enabled in the CPU (EI, IM2 with its own handler) during the measured read
; A1 of S#1. The handler reads S#1 and records that it ran and whether it saw
; FH. Outcomes per e (stored as 4 columns: lost, seen, before, pulse):
;   before: A1 doesn't see FH, the handler does (FH set later, or /INT came
;           before A1)
;   lost:   neither sees FH
;   seen:   A1 sees FH, no interrupt follows
;   pulse:  A1 sees FH and the handler still runs (and finds FH cleared):
;           /INT wasn't active yet at the read, but came anyway
; The width of the "pulse" range is how long /INT comes after FH.
; Use MODE 1 only. (Based on VDPIRQ, MSX2 and up, V9938/V9958.)
; Screen mode A during the sync and the HR samples, B from then on, R#18 and
; R#25 as in VDPMODE. The trial and e are as in VDPMODE.
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG0SAV	equ	0F3DFh
RG1SAV	equ	0F3E0h
RG8SAV	equ	0FFE7h
RG18SAV	equ	0FFF1h
RG19SAV	equ	0FFF2h
RG25SAV	equ	0FFFAh
NPOS	equ	320		; d = 0..319 = 5*q + r
K	equ	48		; HR samples
NE	equ	NPOS + K	; e = 0..NE-1
LOSTA	equ	0C800h		; 16-bit counters per e
SEENA	equ	LOSTA + 2 * NE
BEFA	equ	SEENA + 2 * NE
PULSA	equ	BEFA + 2 * NE
INVAL	equ	PULSA + 2 * NE	; 16-bit count of invalid trials
VTAB	equ	0B000h		; IM2: 257 bytes of 0B1h
ISRADR	equ	0B1B1h		; JP ISR

	output	"VDPIRQH.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	16		; C003h: repetitions per d
coarse:	dw	1075		; C004h: coarse delay loop count (30 cycles each)
hrwait:	db	21		; C006h: delay before the HR samples (18 cycles each)
vdpw:	db	0		; C007h: 1 if VDP I/O has an extra wait cycle (T9769, S1990)
mode:	db	0		; C008h: 0 = S#0/F, 1 = S#1/FH, 2 = S#2/VR, 3 = S#2/HR
line:	db	191		; C009h: R#19 for MODE 1
ma0:	db	04h		; C00Ah: screen mode A, M bits of R#0 (TEXT2: 04h, GRAPHIC4: 06h)
ma1:	db	10h		; C00Bh: screen mode A, M bits of R#1 (TEXT2: 10h, GRAPHIC4: 00h)
mb0:	db	04h		; C00Ch: screen mode B, M bits of R#0
mb1:	db	10h		; C00Dh: screen mode B, M bits of R#1
r18:	db	0FFh		; C00Eh: R#18 during the test (FFh: unchanged)
r25:	db	0FFh		; C00Fh: R#25 during the test (FFh: unchanged)
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
	ld	hl,common.cls	; the classification after the first read
	ld	a,(mode)
	cp	3
	jr	c,.cl
	ld	hl,clshr
.cl:	ld	(common.jp+1),hl
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
	ld	a,(RG1SAV)	; R#1: M1, M2 from MA1/MB1, IE0 off
	and	0C7h
	ld	b,a
	ld	a,(ma1)
	or	b
	ld	(ra1),a
	ld	a,(mb1)
	or	b
	ld	(rb1),a
	di
	ld	hl,VTAB		; the IM2 vector table
	ld	de,VTAB + 1
	ld	bc,256
	ld	(hl),0B1h
	ldir
	ld	a,0C3h
	ld	(ISRADR),a
	ld	hl,isr
	ld	(ISRADR + 1),hl
	ld	a,VTAB / 256
	ld	i,a
	im	2
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
.no25:	ld	a,(mode)
	cp	1
	jr	nz,.nofh
	ld	a,(line)	; MODE 1: R#19 = LINE
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
.nofh:
	ld	hl,LOSTA	; clear the counters
	ld	de,LOSTA + 1
	ld	bc,8 * NE + 2 - 1
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
	call	trial		; A = 0 before, 1 seen, 2 lost, 3 pulse
	cp	3
	ld	hl,PULSA
	jr	z,.cls
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
.nofh2:	im	1		; before CHGCPU, which may enable interrupts
	ld	a,(MSXVER)
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
	in	a,(99h)		; clear the flag of the previous frame
	xor	a
	ld	(isrran),a
	ld	bc,(coarse)
.cd:	dec	bc		; 30 cycles per loop
	ld	a,b
	or	c
	jr	nz,.cd
	ei			; 5 cycles: interrupts on for the read
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
.jp:	jp	.cls		; (patched by MAIN; .cls for MODE 0)
.cls:	ld	b,0
.dl:	djnz	.dl		; ~3600 cycles with interrupts on
	di
	ld	a,c
	and	01h
	jr	z,.nf
	ld	a,(isrran)
	or	a
	ld	a,3
	ret	nz		; pulse
	ld	a,1
	ret			; seen
.nf:	ld	a,(isrran)
	or	a
	jr	z,.lost
	ld	a,(isrf)
	and	01h
	ld	a,0
	ret	nz		; before
.lost:	ld	a,2
	ret			; lost

isr:	push	af		; IM2 handler: S#1 is selected
	in	a,(99h)
	ld	(isrf),a
	ld	a,1
	ld	(isrran),a
	pop	af
	ret			; interrupts stay off

isrran:	db	0
isrf:	db	0

clshr:	ld	a,c		; MODE 3: HR in the first read
	and	20h
	ret	z		; 0: "before"
	ld	a,1
	ret			; 1: "seen"

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
