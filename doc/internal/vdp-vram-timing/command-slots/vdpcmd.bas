10 ' VDPCMD, fully automatic: a 1-byte HMMV started at a known moment
20 ' relative to HR, and which of five reads of S#2 first sees CE = 0.
30 ' Results: per screen S and gap G, K<S><G><V>W<w>.BIN (V: 8 = V9938,
40 ' 9 = V9958), BSAVEd counters: for each class 0..5 the trials per
50 ' e = 0..367 (16 bits each), then the invalid trials; and a
60 ' line per run in LOG.TXT. Screens, all SCREEN 5: G = sprites on,
70 ' H = sprites off, D = display off, R = sprites on with R#18 = 08h,
80 ' Q = sprites on with R#18 = 07h. Gaps 0, 1: the first read 34 or 46
85 ' Z80 cycles after the OUT to R#46. Each screen sets R#8 bit 1 (sprites
88 ' off) itself, as SCREEN keeps it. Existing result files are skipped.
90 CLEAR 300,&H9FFF:SCREEN 0:WIDTH 80:KEY OFF:COLOR 15,4,4
100 BLOAD "VDPCMD.BIN":DEFUSR=&HC000
110 R=12:W=0:IF PEEK(&H2D)>=3 THEN W=1
120 V$="8":IF ((VDP(-1)\2) AND 31)=2 THEN V$="9"
125 ON ERROR GOTO 920:OPEN "LOG.TXT" FOR INPUT AS #1:CLOSE #1:ON ERROR GOTO 0
130 READ S$,G:IF S$="*" THEN 400
140 F$="K"+S$+MID$(STR$(G),2)+V$+"W"+MID$(STR$(W),2)+".BIN"
150 ON ERROR GOTO 900:OPEN F$ FOR INPUT AS #1:CLOSE #1:ON ERROR GOTO 0:PRINT "Skipping ";F$:GOTO 130
160 ON ERROR GOTO 0:PRINT "Screen ";S$;", gap";G;" -> ";F$
180 POKE &HC003,R:POKE &HC004,100:POKE &HC005,0:POKE &HC006,7:POKE &HC007,W:POKE &HC008,G
190 SCREEN 5:IF S$="H" THEN VDP(9)=VDP(9) OR 2 ELSE VDP(9)=VDP(9) AND &HFD
200 IF S$="D" THEN VDP(1)=VDP(1) AND &HBF
210 IF S$="R" THEN VDP(19)=8 ELSE IF S$="Q" THEN VDP(19)=7
220 A=USR(0)
230 V9=VDP(10):V8=VDP(9):V18=VDP(19):VDP(19)=0:VDP(9)=VDP(9) AND &HFD:SCREEN 0:WIDTH 80
240 BSAVE F$,&HA000,&HB141
250 OPEN "LOG.TXT" FOR APPEND AS #1:PRINT #1,F$;" R#8=";V8;" R#9=";V9;" R#18=";V18;" invalid=";PEEK(&HB140)+256*PEEK(&HB141):CLOSE #1
260 GOTO 130
400 RUN "VDPVRAM2.BAS"
900 RESUME 160
920 RESUME 925
925 OPEN "LOG.TXT" FOR OUTPUT AS #1:CLOSE #1:ON ERROR GOTO 0:GOTO 130
1000 DATA G,0,G,1,H,0,H,1,D,0,D,1,R,0,R,1,Q,0,Q,1,*,0
