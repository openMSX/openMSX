10 ' Runs, in this order, what this machine hasn't run yet (no result file):
20 ' VDPNT (N*.TXT), VDPNT2 (FA/FB*.TXT), VDPNT3 (FC*.TXT), VDPNT4
30 ' (GA/GB*.TXT). Each program comes back here when it's done.
40 W=0:IF PEEK(&H2D)>=3 THEN W=1
50 V$="8":IF ((VDP(-1)\2) AND 31)=2 THEN V$="9"
60 S$=V$+"W"+MID$(STR$(W),2)+".TXT"
70 F$="NA"+S$:P$="VDPNT.BAS":GOSUB 100
80 F$="FA"+S$:P$="VDPNT2.BAS":GOSUB 100
85 F$="FC"+S$:P$="VDPNT3.BAS":GOSUB 100
92 F$="GB"+S$:P$="VDPNT4.BAS":GOSUB 100
95 PRINT "All done.":BEEP:END
100 ON ERROR GOTO 130
110 OPEN F$ FOR INPUT AS #1:CLOSE #1
120 ON ERROR GOTO 0:RETURN
130 RESUME 140
140 ON ERROR GOTO 0:PRINT "Running ";P$:RUN P$
