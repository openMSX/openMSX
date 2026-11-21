; VDPCE (MSX2 and up, V9938/V9958): when CE clears after a command, for any
; command, read in 1-cycle steps. Meant for SCREEN 5 with the display off,
; where the command's slots are regular. Each trial:
; - write R#32-R#45 from REGS (through R#17), select S#2, wait for CE = 0,
; - sync on HR's falling edge (S#2 bit 5), send the first byte of the R#46
;   write (the command, REGS+14),
; - OUT (99h),A with 80h+46: the command starts; then a delay of COARSE
;   (optional) plus t = 7..326 cycles, built per trial as code in BUF,
; - MODE 0: IN A,(99h), class 0 (CE = 0) or 1 (CE = 1);
;   MODE 1: write the command to R#46 again (OUT (C),D / OUT (C),E), then
;   IN A,(99h) 27 cycles after the start of the second of those OUTs:
;   class 0 (CE = 0: the second write started nothing that still runs) or
;   1 (CE = 1); then R#46 = 00h (STOP), as a restarted command can be long,
; - count it at i = t - 7. MODE 0: the IN starts 19 + i cycles after the
;   start of the OUT that starts the command; MODE 1: the second R#46 write
;   (OUT (C),E) starts 33 + i cycles after it (both + COARSE's).
; Memory at CNTA: 3 arrays of NI 16-bit counters (class 0, 1, 2).
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
NI	equ	320		; i = 0..319
NCLASS	equ	3
CNTA	equ	0A000h		; NCLASS arrays of NI 16-bit counters
BUF	equ	0B000h		; the timed code, built per trial

	output	"VDPCE.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	20		; C003h: repetitions per i
mode:	db	0		; C004h: 0 = read CE, 1 = start the command again
coarse:	db	0		; C005h: 0, or n: LD L,n / DEC L / JR NZ (8 + 18n - 5 cycles)
	db	0		; C006h: unused
	db	0		; C007h: unused
done:	db	0		; C008h: set to FFh by VDPCE.BAS when all runs are done
	ds	7
regs:	ds	15		; C010h: R#32-R#46: SX, SY, DX, DY, NX, NY, CLR, ARG, CMD
oldcpu:	db	0

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode, update the LED
	call	CHGCPU
.notr:	di
	ld	hl,CNTA		; clear the counters
	ld	de,CNTA + 1
	ld	bc,2 * NI * NCLASS - 1
	ld	(hl),0
	ldir
	ld	a,(reps)
	ld	(repcnt),a
.rep:	ld	hl,0
	ld	(ival),hl
.pos:	call	build
	call	trial		; A = class
	ld	hl,CNTA		; HL = CNTA + A * 2 * NI + 2 * i
	ld	de,2 * NI
	or	a
	jr	z,.cls
	ld	b,a
.ca:	add	hl,de
	djnz	.ca
.cls:	ld	de,(ival)
	add	hl,de
	add	hl,de
	inc	(hl)
	jr	nz,.next
	inc	hl
	inc	(hl)
.next:	ld	hl,(ival)
	inc	hl
	ld	(ival),hl
	ld	de,NI
	or	a
	sbc	hl,de
	jr	nz,.pos
	ld	hl,repcnt
	dec	(hl)
	jr	nz,.rep
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

; build the timed code for t = i + 7 in BUF:
; OUT (99h),A [LD L,n / DEC L / JR NZ,-3] [extra] NOP * q, then
; IN A,(99h) / JP rd (MODE 0), or OUT (C),D / OUT (C),E / LD B,0 / NOP /
; IN A,(99h) / JP rd1 (MODE 1);
; t = 5q + extra, extra: 0, 7 (INC HL), 8 (LD B,0), 9 (DJNZ not taken, B = 1)
; or 11 (LD A,I) for t mod 5 = 0, 2, 3, 4, 1
build:	ld	hl,BUF
	ld	(hl),0D3h	; OUT (99h),A
	inc	hl
	ld	(hl),99h
	inc	hl
	ld	a,(coarse)
	or	a
	jr	z,.nc
	ld	(hl),2Eh	; LD L,n
	inc	hl
	ld	(hl),a
	inc	hl
	ld	(hl),2Dh	; DEC L
	inc	hl
	ld	(hl),20h	; JR NZ,-3
	inc	hl
	ld	(hl),0FDh
	inc	hl
.nc:	push	hl
	ld	hl,(ival)	; DE = t = i + 7, B = q, C = t mod 5
	ld	de,7
	add	hl,de
	ld	b,0
	ld	de,5
.div:	or	a
	sbc	hl,de
	jr	c,.dd
	inc	b
	jr	.div
.dd:	add	hl,de
	ld	c,l
	pop	hl
	ld	a,c
	or	a
	jr	z,.nops
	dec	b		; extras of 7, 8 and 9 take one NOP's 5 cycles
	cp	2
	jr	nz,.e3
	ld	(hl),23h	; INC HL: 7
	inc	hl
	jr	.nops
.e3:	cp	3
	jr	nz,.e4
	ld	(hl),06h	; LD B,0: 8
	inc	hl
	ld	(hl),0
	inc	hl
	jr	.nops
.e4:	cp	4
	jr	nz,.e1
	ld	(hl),10h	; DJNZ +0, not taken (B = 1): 9
	inc	hl
	ld	(hl),0
	inc	hl
	jr	.nops
.e1:	dec	b		; LD A,I: 11 takes two
	ld	(hl),0EDh
	inc	hl
	ld	(hl),57h
	inc	hl
.nops:	ld	a,b
	or	a
	jr	z,.end
.n1:	ld	(hl),0		; NOP: 5
	inc	hl
	djnz	.n1
.end:	ld	a,(mode)
	or	a
	jr	z,.m0
	ld	de,again	; OUT (C),D / OUT (C),E / LD B,0 / NOP
	ex	de,hl
	ld	bc,7
	ldir
	ex	de,hl
.m0:	ld	(hl),0DBh	; IN A,(99h)
	inc	hl
	ld	(hl),99h
	inc	hl
	ld	de,rd
	ld	a,(mode)
	or	a
	jr	z,.jp
	ld	de,rd1
.jp:	ld	(hl),0C3h	; JP rd (MODE 0) or rd1 (MODE 1)
	inc	hl
	ld	(hl),e
	inc	hl
	ld	(hl),d
	ret
again:	db	0EDh, 51h	; OUT (C),D: 14
	db	0EDh, 59h	; OUT (C),E: 14
	db	06h, 0		; LD B,0: 8
	db	0		; NOP: 5

; one trial; returns A = class
trial:	call	cmdset		; (selects S#2)
.h2:	in	a,(99h)		; wait until HR = 1
	and	20h
	jr	z,.h2
.h3:	in	a,(99h)		; wait until HR = 0
	and	20h
	jr	nz,.h3
	ld	a,(regs + 14)	; first byte of the R#46 write: the command
	out	(99h),a
	ld	d,a		; for MODE 1: the command again, through port C
	ld	e,80h + 46
	ld	c,99h
	ld	b,1		; for the DJNZ in BUF
	ld	a,e
	jp	BUF

rd:	and	1		; CE
	ld	(rdbk),a
	call	cewait
	ld	a,(rdbk)
	ret

rd1:	and	1		; CE
	ld	(rdbk),a
	xor	a		; STOP whatever runs: a restarted command can be long
	out	(99h),a
	ld	a,80h + 46
	out	(99h),a
	call	cewait
	ld	a,(rdbk)
	ret

; wait until CE = 0 (S#2 is selected)
cewait:	in	a,(99h)
	rra
	jr	c,cewait
	ret

; R#32-R#45 from REGS (through R#17), select S#2, wait for CE = 0
cmdset:	ld	a,32		; R#17 = 32, auto-increment
	out	(99h),a
	ld	a,80h + 17
	out	(99h),a
	ld	hl,regs
	ld	bc,14 * 256 + 9Bh
	otir
	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	jp	cewait

repcnt:	db	0
rdbk:	db	0
ival:	dw	0
end:
