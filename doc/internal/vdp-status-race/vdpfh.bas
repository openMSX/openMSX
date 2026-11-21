10 ' VDPFH: where a write timed with the line interrupt lands in the picture.
20 ' Photo, then space.
30 CLEAR 200,&HAFFF:BLOAD "VDPFH.BIN"
40 W=0:IF PEEK(&H2D)>=3 THEN W=1
50 POKE &HC003,12:POKE &HC004,0:POKE &HC005,100:POKE &HC006,W:POKE &HC007,64
60 ' predicted first and last stripe pixel: openMSX now, and with the picture
70 ' 33 VDP cycles later; FS-A1GT if W=1, else HB-F700D
80 IF W=1 THEN M1=57:M2=227:P1=48:P2=218 ELSE M1=55:M2=222:P1=47:P2=214
90 COLOR 15,0,4:SCREEN 5:OPEN "GRP:" FOR OUTPUT AS #1
100 PSET (8,8),0:PRINT #1,"VDPFH SCREEN 5 (line interrupt)"
110 PSET (8,24),0:PRINT #1,"red: openMSX now"
120 PSET (8,36),0:PRINT #1,"green: picture 33 cycles later"
130 PSET (8,52),0:PRINT #1,"Photo, then space"
140 FOR X=0 TO 1:LINE (M1+X*(M2-M1),76)-(M1+X*(M2-M1),98),9:LINE (M1+X*(M2-M1),166)-(M1+X*(M2-M1),188),9:NEXT
150 FOR X=0 TO 1:LINE (P1+X*(P2-P1),76)-(P1+X*(P2-P1),98),3:LINE (P1+X*(P2-P1),166)-(P1+X*(P2-P1),188),3:NEXT
160 POKE &HC00B,5:POKE &HC008,4:POKE &HC009,15:DEFUSR=&HC000:A=USR(0)
170 CLOSE:SCREEN 0:PRINT "Done."
