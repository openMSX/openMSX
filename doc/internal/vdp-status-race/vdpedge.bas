10 ' VDPEDGE: VR, HR and the VRAM slots relative to the HR edge
20 CLEAR 200,&HBFFF:SCREEN 0:WIDTH 80:KEY OFF:COLOR 15,4,4
30 BLOAD "VDPEDGE.BIN"
40 R=32:H=21
42 INPUT "Mode (0 = VR rise, 1 = VR fall, 2 = HR, 3 = VRAM writes, 4 = VRAM read back)";M
43 SC=0:IF M<>3 THEN INPUT "Screen (0 = SCREEN 0 WIDTH 80, 5 = SCREEN 5)";SC
44 INPUT "Extra VDP I/O wait (1 = turboR or Panasonic MSX2+, 0 = other)";W
45 IF SC=5 THEN H=20
46 C=100:IF M=0 THEN C=1075:IF SC=5 THEN C=1227
47 IF M=1 THEN IF (VDP(10) AND 2) THEN C=1987 ELSE C=1599
48 F$="V6M"+MID$(STR$(M),2)+"S"+MID$(STR$(SC),2)+"W"+MID$(STR$(W),2)+".TXT"
50 N=368:LA=&HC800:SA=LA+2*N:BA=SA+2*N:IA=BA+2*N
60 PRINT "VDP edge test, M=";M;", screen";SC;", R=";R;", C=";C;", H=";H;", W=";W
70 PRINT "Takes about";INT(R*320/50*(1-(M=1)))+1;"seconds (less at 60 Hz). Please wait..."
75 IF SC=5 THEN SCREEN 5
80 POKE &HC003,R:POKE &HC004,C AND 255:POKE &HC005,C\256:POKE &HC006,H:POKE &HC007,W:POKE &HC008,M
90 DEFUSR=&HC000:A=USR(0)
95 V9=VDP(10):SCREEN 0:WIDTH 80
100 PRINT "e=0..";N-1;" (1 Z80 cycle per character), M=";M;", screen";SC;", C=";C
110 IF M>=3 THEN PRINT "+ both written, * second write lost, ? other, x mixed, . none" ELSE PRINT "+ bit is 1, - bit is 0, x mixed, . none"
130 FOR E=0 TO N-1
140 L=PEEK(LA+2*E)+256*PEEK(LA+2*E+1):S=PEEK(SA+2*E)+256*PEEK(SA+2*E+1):B=PEEK(BA+2*E)+256*PEEK(BA+2*E+1)
210 C$="x":IF L+S+B=0 THEN C$="." ELSE IF L+B=0 THEN C$="+" ELSE IF S+B=0 THEN C$="*" ELSE IF L+S=0 THEN C$="-":IF M>=3 THEN C$="?"
230 PRINT C$;
240 NEXT
260 PRINT:PRINT "invalid trials:";PEEK(IA)+256*PEEK(IA+1);" of";R*320
280 OPEN F$ FOR OUTPUT AS #1
290 PRINT #1,"VDPEDGE R=";R;" C=";C;" H=";H;" W=";W;" M=";M;" SC=";SC;" R#9=";V9;" invalid=";PEEK(IA)+256*PEEK(IA+1)
300 FOR E=0 TO N-1
310 L=PEEK(LA+2*E)+256*PEEK(LA+2*E+1):S=PEEK(SA+2*E)+256*PEEK(SA+2*E+1):B=PEEK(BA+2*E)+256*PEEK(BA+2*E+1)
320 IF L+S+B>0 THEN PRINT #1,E;L;S;B
330 NEXT
335 IF M=4 THEN HN=PEEK(&HD0A2):PRINT #1,"triples";HN:FOR J=0 TO HN-1:HA=&HD0A3+5*J:PRINT #1,"T";PEEK(HA);PEEK(HA+1);PEEK(HA+2);PEEK(HA+3)+256*PEEK(HA+4):NEXT
340 CLOSE #1:PRINT "Saved: ";F$;" (e, lost/zero, one, other/zero)"
