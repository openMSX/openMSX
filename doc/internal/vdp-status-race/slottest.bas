10 ' SLOTTEST: F with the display off, FH in a border line (see README.md)
20 CLEAR 200,&HBFFF:SCREEN 0:WIDTH 80:KEY OFF:COLOR 15,4,4
30 BLOAD "SLOTTEST.BIN"
40 R=32:H=21:Z=0
42 INPUT "Test (1 = F with the display off, 2 = FH at line 200)";T
44 INPUT "Extra VDP I/O wait (1 = turboR or Panasonic MSX2+, 0 = other)";W
46 IF T=1 THEN M=0:C=1075:Y=191:K=1 ELSE M=1:C=1143:Y=200:K=0
47 F$="V5T"+MID$(STR$(T),2)+"W"+MID$(STR$(W),2)+".TXT"
45 IF Z=60 THEN VDP(10)=VDP(10) AND 253 ELSE IF Z=50 THEN VDP(10)=VDP(10) OR 2
50 N=368:LA=&HC800:SA=LA+2*N:BA=SA+2*N:IA=BA+2*N
60 PRINT "Slot test";T;"(M=";M;", Y=";Y;", display off=";K;"), R=";R;", C=";C;", W=";W
70 PRINT "Takes about";INT(R*320/50)+1;"seconds (less at 60 Hz). Please wait..."
80 POKE &HC003,R:POKE &HC004,C AND 255:POKE &HC005,C\256:POKE &HC006,H:POKE &HC007,W:POKE &HC008,M:POKE &HC009,Y:POKE &HC00A,K
90 DEFUSR=&HC000:A=USR(0)
100 CLS:PRINT "e=0..";N-1;" (1 Z80 cycle per character), R=";R;", C=";C;", H=";H;", W=";W;", Z=";Z
110 PRINT "- before the flag was set, + seen, * lost, ~ before/seen, x lost and others, . none"
120 TL=0:FL=-1:LL=-1:FS=-1:LB=-1:FE=-1:LE=-1
130 FOR E=0 TO N-1
140 L=PEEK(LA+2*E)+256*PEEK(LA+2*E+1):S=PEEK(SA+2*E)+256*PEEK(SA+2*E+1):B=PEEK(BA+2*E)+256*PEEK(BA+2*E+1)
150 TL=TL+L:IF L+S+B>0 THEN IF FE<0 THEN FE=E
160 IF L+S+B>0 THEN LE=E
170 IF L>0 THEN IF FL<0 THEN FL=E
180 IF L>0 THEN LL=E
190 IF S>0 AND FS<0 THEN FS=E
200 IF B>0 THEN LB=E
210 IF L+S+B=0 THEN C$="." ELSE IF L=0 THEN IF S=0 THEN C$="-" ELSE IF B=0 THEN C$="+" ELSE C$="~"
220 IF L>0 THEN IF S+B=0 THEN C$="*" ELSE C$="x"
230 PRINT C$;
240 NEXT
250 PRINT:PRINT "data e";FE;"-";LE;", lost trials";TL;", lost e";FL;"-";LL;", seen from e";FS;", before until e";LB
260 PRINT "invalid trials:";PEEK(IA)+256*PEEK(IA+1);" of";R*320
270 IF FS<0 THEN PRINT "F never seen: increase C in line 40 (1 = 30 cycles)" ELSE IF LB<0 THEN PRINT "Never before F: decrease C in line 40 (1 = 30 cycles)"
280 OPEN F$ FOR OUTPUT AS #1
290 PRINT #1,"R=";R;" C=";C;" H=";H;" W=";W;" Z=";Z;" T=";T;" M=";M;" Y=";Y;" K=";K;" R#9=";VDP(10);" invalid=";PEEK(IA)+256*PEEK(IA+1)
300 FOR E=0 TO N-1
310 L=PEEK(LA+2*E)+256*PEEK(LA+2*E+1):S=PEEK(SA+2*E)+256*PEEK(SA+2*E+1):B=PEEK(BA+2*E)+256*PEEK(BA+2*E+1)
320 IF L+S+B>0 THEN PRINT #1,E;L;S;B
330 NEXT
340 CLOSE #1:PRINT "Saved: ";F$;" (e, lost, seen, before)"
