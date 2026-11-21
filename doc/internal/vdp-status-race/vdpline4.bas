10 ' VDPLINE 4: R#23 (vertical scroll) written relative to HR, SCREEN 5, in
20 ' 8 groups of 8 lines: in each next group the write comes 1 Z80 cycle
30 ' later. Over blocks of 8 white and 8 black lines the write shows as a
40 ' seam; the seams of the groups form a staircase. Ruler: a tick every 8
50 ' pixels, longer every 32. Photo, then space.
60 CLEAR 300,&HBFFF:BLOAD "VDPLIN4.BIN":DEFUSR=&HC000
70 W=0:IF PEEK(&H2D)>=3 THEN W=1
80 POKE &HC003,376 AND 255:POKE &HC004,376\256:POKE &HC005,21:POKE &HC006,W:POKE &HC007,64:POKE &HC00C,2:POKE &HC012,23
90 COLOR 15,0,0:SCREEN 5:CLS:OPEN "GRP:" FOR OUTPUT AS #1
100 PSET (8,8),0:PRINT #1,"VDPLINE R#23 staircase, SCREEN 5"
110 PSET (8,24),0:PRINT #1,"1 Z80 cycle later per 8 lines"
120 PSET (8,40),0:PRINT #1,"Photo, then space":CLOSE #1
130 FOR Y=96 TO 167 STEP 16:LINE (0,Y)-(255,Y+7),15,BF:NEXT
140 FOR X=0 TO 255 STEP 8:L=3:IF X MOD 32=0 THEN L=8
150 LINE (X,90-L)-(X,90),11:LINE (X,176)-(X,176+L),11:NEXT
160 POKE &HC00B,1:POKE &HC008,0:POKE &HC009,8:A=USR(0)
170 SCREEN 0:PRINT "Done."
