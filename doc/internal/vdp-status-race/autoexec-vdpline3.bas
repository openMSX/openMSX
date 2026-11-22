10 ' CLKTEST, then VDPLINE 3. No input needed until the photos.
20 CLEAR 200,&HBFFF:SCREEN 0:WIDTH 80:KEY OFF
30 BLOAD "CLKTEST.BIN"
40 W=0:IF PEEK(&H2D)>=3 THEN W=1
50 N=64:POKE &HC003,N:POKE &HC007,W
60 PRINT "CLKTEST: measuring";N;"trials (a few seconds)..."
70 DEFUSR=&HC000:A=USR(0)
80 S=0:V=0:MI=999:MA=-999
90 F$="CLKW"+MID$(STR$(W),2)+".TXT":OPEN F$ FOR OUTPUT AS #1
100 PRINT #1,"W=";W;" N=";N;" (C1 C2 per trial, 313 lines = 71364 CPU cycles apart)"
110 FOR I=0 TO N-1
120 C1=PEEK(&HC200+2*I):C2=PEEK(&HC201+2*I):PRINT #1,C1;C2
130 IF C1>0 AND C1<64 AND C2>0 AND C2<64 THEN D=C2-C1:S=S+D:V=V+1:IF D<MI THEN MI=D
140 IF C1>0 AND C1<64 AND C2>0 AND C2<64 THEN IF D>MA THEN MA=D
150 NEXT:CLOSE #1
160 IF V=0 THEN PRINT "No valid trials":GOTO 200
170 PRINT "valid trials:";V;" of";N;", C2-C1: min";MI;" max";MA;" mean";INT(S/V*100)/100
180 PRINT "difference per 71364 CPU cycles:";INT(S/V*1E6/71364!+.5);"ppm"
190 PRINT "Saved: ";F$
200 PRINT "Now VDPLINE 3 (three screens: photo, then space). Press space to start."
210 IF INKEY$<>" " THEN 210
220 RUN "VDPLINE3.BAS"
