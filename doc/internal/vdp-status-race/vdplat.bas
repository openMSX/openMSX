10 ' VDPLAT: the line latch with the horizontal adjust. VDPYJKN.BIN (16 groups
20 ' of 8 lines, 1 Z80 cycle later per group) writes R#1: display off, and on
30 ' again 50 Z80 cycles later, over a white picture, in SCREEN 8, with R#18 =
40 ' 00h, 04h and 0Ch (set before the HR samples, so the writes move along with
50 ' HR). A line stays black when the latch for it comes while the display is
60 ' off. If the latch moves with the adjust as HR does, the black band is the
70 ' same in all three screens. Photo or video, then space.
80 CLEAR 300,&H9FFF
90 W=0:IF PEEK(&H2D)>=3 THEN W=1
100 V$="8":IF ((VDP(-1)\2) AND 31)=2 THEN V$="9"
110 OPEN "LT"+V$+"W"+MID$(STR$(W),2)+".TXT" FOR OUTPUT AS #1:PRINT #1,"VDPLAT started":CLOSE #1
120 BLOAD "VDPYJKN.BIN":DEFUSR=&HA000
130 FOR I=1 TO 3:RA=0:IF I=2 THEN RA=4 ELSE IF I=3 THEN RA=12
140 SCREEN 8:COLOR 255,0,0:CLS:VDP(9)=VDP(9) OR 32
150 LINE (0,0)-(255,38),0,BF:OPEN "GRP:" FOR OUTPUT AS #1
160 PSET (8,8),0:PRINT #1,"VDPLAT, screen";I;"of 3"
170 PSET (8,20),0:PRINT #1,"SCREEN 8, R#18 =";RA;", latch":CLOSE #1
180 LINE (0,44)-(255,211),&HF8,BF
190 C=37:CA=PEEK(&HF3E0) OR &H40:CB=CA AND &HBF
200 POKE &HA003,C AND 255:POKE &HA004,C\256:POKE &HA005,21:POKE &HA006,W:POKE &HA007,128:POKE &HA00C,2:POKE &HA012,1
210 POKE &HA013,RA:POKE &HA014,255:POKE &HA00B,I:POKE &HA008,CA:POKE &HA009,CB:A=USR(0)
220 NEXT
230 SCREEN 0:PRINT "VDPLAT done.":RUN "AUTOEXEC.BAS"
