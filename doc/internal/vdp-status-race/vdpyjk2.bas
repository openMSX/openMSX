10 ' VDPYJK 2 (V9958): YJK edges, sprites in YJK, a mid-line YJK switch.
20 ' Screen 1: top SCREEN 8, bottom SCREEN 12 (VDPYJK.BIN), the same columns
30 ' (x = 0, every 16, 255) and red sprite bars 3 pixels wide, 3 pixels
35 ' left of the columns at x = 64, 128 and 192: do sprites move with the YJK picture? Is x = 255 cut off?
50 ' Screens 2 and 3: SCREEN 8 and 12, a blue backdrop stripe whose left
60 ' edge sweeps through the display start, 16 groups of 8 lines, 1 Z80
70 ' cycle per group (VDPYJKL.BIN): what do the 4 pixels before the first
80 ' YJK pixel show? Screen 4: SCREEN 8, R#25 = 08h (YJK) written in the
90 ' middle of the line and 00h later, 1 Z80 cycle later per 8 lines
100 ' (VDPLIN4.BIN). Photo, then space for the next screen.
110 CLEAR 300,&H9FFF:DEFUSR=&HC000
120 IF ((VDP(-1)\2) AND 31)<>2 THEN PRINT "Not a V9958.":END
130 W=0:IF PEEK(&H2D)>=3 THEN W=1
140 ' screen 1
150 BLOAD "VDPYJK.BIN"
160 SCREEN 8,2:COLOR 255,0,3:CLS:VDP(9)=VDP(9) OR 32
170 T$="VDPYJK 2, screen 1 of 4":U$="top 8, bottom 12; whole + edges":GOSUB 1000
180 FOR X=0 TO 240 STEP 16:LINE (X,40)-(X,200),&HF8:NEXT:LINE (255,40)-(255,200),&HF8
190 SPRITE$(0)=STRING$(16,224)+STRING$(16,0)
200 FOR I=0 TO 2:FOR J=0 TO 1:FOR K=0 TO 1
210 PUT SPRITE I*4+J*2+K,(58+64*I,50+J*80+K*16),10,0
220 NEXT K,J,I
230 POKE &HC005,1:A=USR(0)
240 ' screens 2 and 3
250 BLOAD "VDPYJKL.BIN":DEFUSR=&HA000
260 C=36:POKE &HA003,C AND 255:POKE &HA004,C\256:POKE &HA005,21:POKE &HA006,W:POKE &HA007,128:POKE &HA00C,2:POKE &HA012,7
270 SCREEN 8:COLOR 255,0,0:CLS:VDP(9)=VDP(9) OR 32
280 FOR X=0 TO 240 STEP 16:LINE (X,44)-(X,211),&HF8:NEXT:LINE (255,44)-(255,211),&HF8
290 R5=0:IF PEEK(&H2D)>=2 THEN R5=PEEK(&HFFFA)
300 FOR I=2 TO 3
310 T$="VDPYJK 2, screen"+STR$(I)+" of 4":U$="Photo of the LEFT border":GOSUB 1000
320 V=R5 AND &HE7:IF I=3 THEN V=V OR 8
330 POKE &HA013,8:POKE &HA014,V
340 POKE &HA00B,I:POKE &HA008,&H1F:POKE &HA009,3:A=USR(0)
350 NEXT
360 ' screen 4
370 BLOAD "VDPLIN4.BIN":DEFUSR=&HC000
380 POKE &HC003,376 AND 255:POKE &HC004,376\256:POKE &HC005,21:POKE &HC006,W:POKE &HC007,64:POKE &HC00C,2:POKE &HC012,25
390 SCREEN 8:COLOR 255,0,0:CLS:VDP(9)=VDP(9) OR 32
400 T$="VDPYJK 2, screen 4 of 4":U$="YJK on mid-line, whole screen":GOSUB 1000
410 FOR X=0 TO 255 STEP 8:LINE (X,44)-(X,211),&HF8:NEXT
420 FOR X=0 TO 255 STEP 32:LINE (X+1,44)-(X+1,211),&HF8:NEXT
430 POKE &HC00B,4:POKE &HC008,0:POKE &HC009,8:A=USR(0)
440 SCREEN 0:PRINT "Done."
450 END
1000 LINE (0,0)-(255,38),0,BF:OPEN "GRP:" FOR OUTPUT AS #1
1010 PSET (8,8),0:PRINT #1,T$:PSET (8,20),0:PRINT #1,U$:CLOSE #1:RETURN
