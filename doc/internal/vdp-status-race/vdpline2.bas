10 ' VDPLINE 2: where a write timed relative to HR lands in the picture.
20 ' Each screen: take a photo, then press space.
30 CLEAR 300,&HBFFF:BLOAD "VDPLINE.BIN":DEFUSR=&HC000
40 W=0:IF PEEK(&H2D)>=3 THEN W=1
50 POKE &HC005,21:POKE &HC006,W:POKE &HC007,64
60 FOR SC=1 TO 2
70 READ T$,L1$,L2$,MD,EF,CO:FOR I=0 TO 1:FOR J=0 TO 3:READ P(I,J):NEXT:NEXT
80 POKE &HC003,CO AND 255:POKE &HC004,CO\256:POKE &HC00C,EF
90 IF MD=5 THEN GOSUB 200 ELSE GOSUB 300
100 POKE &HC00B,SC:A=USR(0)
110 NEXT
120 CLOSE:SCREEN 0:WIDTH 80:COLOR 15,4,4:PRINT "Photos done."
130 END
200 ' SCREEN 5: red = the first marker pair, green = the second
210 COLOR 15,0,4:SCREEN 5:OPEN "GRP:" FOR OUTPUT AS #1
220 PSET (8,8),0:PRINT #1,T$
230 PSET (8,24),0:PRINT #1,"red: ";L1$
240 PSET (8,36),0:PRINT #1,"green: ";L2$
250 PSET (8,52),0:PRINT #1,"Photo, then space"
260 FOR J=0 TO 3:X=P(W,J):IF X<0 THEN 280
270 C=9:IF J>1 THEN C=3
275 LINE (X,76)-(X,98),C:LINE (X,166)-(X,188),C
280 NEXT
290 POKE &HC008,4:POKE &HC009,15:RETURN
300 ' text mode: solid = the first marker pair, dotted = the second
310 CLOSE:COLOR 15,4,4:SCREEN 0:WIDTH MD:KEY OFF:CLS
320 PRINT T$:PRINT "solid: ";L1$;", dotted: ";L2$:PRINT "Photo, then space"
330 O=18:PW=6:IF MD=40 THEN O=9
340 FOR J=0 TO 3:X=P(W,J):IF X<0 THEN 390
350 X=X-O:C=X\PW:B=2^(7-(X MOD PW)):CH=252+J:A=BASE(2)+8*CH
360 FOR K=0 TO 7:V=B:IF J>1 AND (K AND 1) THEN V=0
370 VPOKE A+K,V:NEXT
380 FOR R=9 TO 11:LOCATE C,R:PRINT CHR$(CH);:NEXT:FOR R=21 TO 23:LOCATE C,R:PRINT CHR$(CH);:NEXT
390 NEXT
395 POKE &HC008,&HF4:POKE &HC009,&HF8:RETURN
1000 ' per screen: title, the two marker legends, mode (5, 80 or 40), effect (0 R#7,
1010 ' 1 palette), coarse, then the markers (first and last stripe pixel of both
1020 ' pairs; -1 = none) for the HB-F700D (W=0), then for the FS-A1GT (W=1)
1040 DATA "SCREEN 0 WIDTH 80, R#7 write","openMSX","openMSX - 33 cycles",80,0,375, 102,437,86,421, 105,446,89,430
1060 DATA "SCREEN 0 WIDTH 40, R#7 write","openMSX","openMSX - 33 cycles",40,0,375, 51,218,43,210, 52,222,44,214
