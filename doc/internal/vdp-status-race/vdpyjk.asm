; VDPYJK: does YJK move the picture? (V9958)
; Each frame, the top of the screen is SCREEN 8 (R#25 = 00h) and from line
; LINE on SCREEN 12 (R#25 = 08h, YJK), switched by the line interrupt (IE1;
; IM2 with its own vector table, the CPU waits with HALT). The caller draws
; pixels with value F8h: yellow in SCREEN 8 and white in SCREEN 12 (Y = 31, J = K = 0), so both halves show
; the same columns. If YJK moves the picture, the lower half is shifted.
; Space (checked once per frame) stops it.

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
RG0SAV	equ	0F3DFh
RG1SAV	equ	0F3E0h
RG19SAV	equ	0FFF2h
RG25SAV	equ	0FFFAh
VTAB	equ	0B000h		; IM2: 257 bytes of 0B1h
ISRADR	equ	0B1B1h		; JP ISR

	output	"VDPYJK.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
line:	db	106		; C003h: R#19, the first SCREEN 12 line
oldcpu:	db	0
marker:	db	0		; C005h: free for the caller (openMSX test script)

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode
	call	CHGCPU
.notr:	di
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
.rel:	in	a,(0AAh)	; wait until space is released
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jr	nc,.rel
	ld	a,(line)	; R#19 = LINE, IE1 on
	out	(99h),a
	ld	a,80h + 19
	out	(99h),a
	ld	a,(RG0SAV)
	or	10h
	out	(99h),a
	ld	a,80h + 0
	out	(99h),a
	ld	a,(RG1SAV)	; R#1: IE0 off, only the line interrupt
	and	0DFh
	out	(99h),a
	ld	a,80h + 1
	out	(99h),a

frame:	ld	a,2		; S#2: wait for VR = 1
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
.vr:	in	a,(99h)
	and	40h
	jr	z,.vr
	xor	a		; R#25 = 00h: SCREEN 8 at the top
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
	ld	a,1		; S#1: clear FH
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	in	a,(99h)
	ld	b,10		; the interrupt line drops some cycles after the
.w:	djnz	.w		; read that clears FH: wait before EI
	ei
	halt			; -> ISR at line LINE
	in	a,(0AAh)	; space bar: keyboard row 8, bit 0
	and	0F0h
	or	8
	out	(0AAh),a
	in	a,(0A9h)
	rrca
	jr	c,frame
	xor	a		; S#0, restore R#0, R#19, R#25
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(RG0SAV)
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
	ld	a,(RG25SAV)
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
	im	1		; before CHGCPU, which may enable interrupts
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret
isr:	push	af		; R#25 = 08h: SCREEN 12 from here, clear FH
	ld	a,08h
	out	(99h),a
	ld	a,80h + 25
	out	(99h),a
	in	a,(99h)		; S#1 is selected
	pop	af
	ret			; interrupts stay off
end:
