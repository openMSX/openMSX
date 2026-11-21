10 ' VDPLINE 5: does YJK move the VDP's timing, or only its pixels? (V9958)
20 ' A backdrop stripe timed relative to HR, from the right border into the
30 ' left border of the next line, in 8 groups 1 Z80 cycle apart. Columns of
40 ' pixel value F8h every 16 pixels and at x=255. Four screens, each with the
50 ' HR samples taken in its own mode: SCREEN 8 (yellow columns) and SCREEN 12
60 ' (white columns), with R#18 = 08h (picture 8 pixels right, for the left
70 ' border) and 00h (for the right border). Photo, then space for the next.
80 CLEAR 300,&HBFFF:BLOAD "VDPLIN5.BIN":DEFUSR=&HC000
90 IF ((VDP(-1)\2) AND 31)<>2 THEN PRINT "Not a V9958.":END
100 W=0:IF PEEK(&H2D)>=3 THEN W=1
110 C=378
120 POKE &HC003,C AND 255:POKE &HC004,C\256:POKE &HC005,21:POKE &HC006,W:POKE &HC007,64:POKE &HC00C,2:POKE &HC012,7
130 SCREEN 8:COLOR 255,0,0:CLS
140 FOR X=0 TO 240 STEP 16:LINE (X,44)-(X,211),&HF8:NEXT:LINE (255,44)-(255,211),&HF8
150 R5=0:IF PEEK(&H2D)>=2 THEN R5=PEEK(&HFFFA)
160 FOR I=1 TO 4:READ J,Y,B$
170 LINE (0,0)-(255,40),0,BF:OPEN "GRP:" FOR OUTPUT AS #1
180 PSET (8,8),0:PRINT #1,"VDPLINE 5, screen";I;"of 4"
190 PSET (8,20),0:PRINT #1,"Photo of the ";B$;" border":CLOSE #1
210 V=R5 AND &HE7:IF Y THEN V=V OR 8
220 POKE &HC013,J:POKE &HC014,V
230 POKE &HC00B,I:POKE &HC008,0:POKE &HC009,3:A=USR(0)
240 NEXT
260 SCREEN 0:PRINT "Done."
270 END
1000 ' screens: R#18, YJK (0 = SCREEN 8, 1 = SCREEN 12), border to photograph
1010 DATA 8,0,LEFT, 8,1,LEFT, 0,0,RIGHT, 0,1,RIGHT
