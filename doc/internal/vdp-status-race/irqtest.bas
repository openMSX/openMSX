10 ' IRQTEST: are VDP interrupts lost while S#0 is polled? (turboR)
20 CLEAR 200,&HBFFF:SCREEN 0:WIDTH 80:KEY OFF
30 BLOAD "IRQTEST.BIN"
40 W=40:Z=60
50 PRINT "IRQ test: polling S#0 with interrupts on for";INT(W*65536!*14/3579545!*10)/10;"seconds..."
60 POKE &HC003,W:DEFUSR=&HC000:A=USR(0)
70 DEF FNW(A)=PEEK(A)+256*PEEK(A+1)
80 I=FNW(&HC010):F=FNW(&HC012):M=FNW(&HC014):T=W*65536!+FNW(&HC016)
90 IF Z=60 THEN FR=T*14/59736! ELSE FR=T*14/71364!
100 PRINT "E6 ticks:";T;" frames:";INT(FR*10)/10
110 PRINT "interrupts:";I;" (handler saw F:";F;"), F seen by the main loop:";M
120 PRINT "lost (no interrupt, main loop didn't see F either):";INT((FR-I-M)*10)/10
130 OPEN "IRQ.TXT" FOR OUTPUT AS #1
140 PRINT #1,"W=";W;" Z=";Z;" ticks=";T;" frames=";FR;" interrupts=";I;" handlerF=";F;" mainF=";M
150 CLOSE #1:PRINT "Saved: IRQ.TXT"
