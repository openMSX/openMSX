# Plotter information

## Sony PRN-C41(D) / General PCP-5501 / Toshiba HX-P570
This information is copied from the original Sony manual and some parts are from the Creative Greetings software manual. The Sony plotter was release with Plotter (Creative Greetings) software. Also some add on graphics are released for this software, see also: https://www.generation-msx.nl/group/sony-plotter-series/97/

### Control Characters

| Name            | Code                | Functions                                                                                   |
|-----------------|--------------------|---------------------------------------------------------------------------------------------|
| Carriage return | ODH                 | When DIP switch 4 is ON: pen moves to top of the next line. OFF: pen moves to top of the line. |
| Line feed       | OAH                 | Feeds a line.                                                                               |
| Back space      | 08H                 | Moves pen one character space back (ignored at the top of the line).                        |
| Line up         | OBH                 | Feeds paper to the previous line.                                                           |
| Graphic mode    | 1BH+"#"             | Sets the plotter printer in the graphic mode.                                               |
| Scale set       | 12H+"0"−"15"        | Sets the size of characters (0 is the smallest size).                                       |
| Color set       | 1BH+"C"+"0"−"3"     | Sets the pen color (0: black, 1: blue, 2: green, 3: red).                                   |
| Top of form     | OCH                 | Feeds paper 297 mm (1143​ inches) down from the printing start position.                    |
| Text mode       | 1BH+"$"             | Sets the plotter printer in the text mode.                                                  |

### Graphic Mode Instruction Codes Table

| Category                  | Command                                         | Function                                                                 | Remarks / Examples                                                                                       |
|---------------------------|-------------------------------------------------|--------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| Pen Point Position        | A (All initialize)                              | Moves pen to the leftmost position, sets it as the origin, and sets the plotter printer to text mode.     | Commands I, H, A, and F can be followed by another command without inserting any characters.              |
|                           | F (New line)                                   | Moves the pen to the top of the next line.                             |  LPRINT "HD100,0"                                                                                                        |
|                           | H (Home)                            | Moves the pen to the origin without drawing a line.|   Draws a line from the origin to (100,0)                                                                                                       |
|                         | I (Initialize)                                 | Sets the current pen position as the origin (the intersection of the x and y axes) |                                                                                                          |
| Printing Design / Drawing | Cn (n=0−3) (Color change)                      | Sets the color (0:black, 1:blue, 2:green, 3:red).                      | Commands C, S, Q, and L can be followed by another command by inserting a comma between them.             |
|                           | Ln (n=0−15) (Line type)                        | Selects solid (n=0,15) or broken lines (n=1−14). Higher n means rougher broken line. |                                                                                                          |
|                           | Qn (n=0−3) (Alpha rotate)                      | Sets the direction of characters to be printed out (0, 1, 2, 3).        |                                                                                                          |
|                           | Sn (n=0−15) (Scale set)                        | Sets the size of characters to be printed out.                          |                                                                                                          |
|                           | D x1, y1. x2, y2... (Draw)                     | Draws a line from current position to (x1,y1), then to (x2,y2), etc..   | Other commands cannot be followed by another command. Use separate LPRINT statements for sequence.        |
|                           | J $\Delta x1, \Delta y1, \Delta x2, \Delta y2...$ (Relative draw) | Draws a line from the current position Δx1 horizontally and Δy1 vertically, then Δx2 and Δy2, etc.. | The pen can be moved up to +2047 and −2048 steps in the vertical directions with commands J and R. If exceeded, the printer is reset. |
|                           | M x, y (Move)                                  | Moves the pen to position (x,y).                                        |                                                                                                          |
|                           | R $\Delta x, \Delta y$ (Relative move)         | Moves the pen Δx horizontally and Δy vertically.                        |                                                                                                          |
| Character Print           | P chrs. (Print)                                | Prints out characters.                                                  | Any spaces specified after the commands are ignored.                                                      |


### Paper size and Plotting Area

| Size | Description | Dimensions (mm) | Plotting Area (mm) |
|------|-------------|------------------|---------------------|
| Card | Picture postcard size | 150x100 | about 90x60 |
| Roll | The roll paper that comes with the plotter | 114 (width) | about 110x80 |
| A5 | The size of a page in this book | 210x148 | about 140x95 |
| B5 | Notebook paper size | 257x182 | about 180x120 |
| A4 | Typing paper size | 296x210 | about 210x140 |

### Graphics printing area
|direction/paper size|100mm (Card) | 114 mm (Roll) | A5 | B5 | A4 |
|---|---|---|---|---|---|
|x-direction| 410 steps (82mm) | 480 steps (96mm) | 650 steps (130mm) | 820 steps (164mm) | 960 steps (192mm) |
|+y-direction| 30 steps (6mm) | - | 30 steps (6mm) | 30 steps (6mm) | 30 steps (6mm)|
|-y-direction| 601 steps (120.2mm) | - | 919 steps (183.8mm) | 1149 steps (229.8mm) | 1354 steps (270.8mm)|



### Print functions:
|Function|Description|
|---|---|
|Print system|Ball point pen print, 4 colors rotary system|
|Driving system|Hybrid x-y plotter|
|Graphics print speed|x, y: 57 mm/sec|
|Resolution|0.2mm/step|
|Character types|252 types|
|Character size|0.8 x 1.2 mm to 12.8x19.2mm (16 sizes)|
|Number of characters to a line|Max 160 characters (paper size: A4 with minimum character size)|
|Character pitch|2.4mm|
|Character direction| 4 directions |
|Colors| 4 colors (black, blue, green, red)|
|Sign| MSX specification Character code |
|Precision of repeated printing| less than 0.2mm|
|Precision of distance | less +- 1.0% in x and y axes|

When printing the test page, it prints 80 characters per line on real A4 paper.

### Dip Switches
The printer has four dip switches, which are by factory default all off.

Switch 1 and 2 are not connected at all according to the service manual.

Switch 3 is only connected on the Japanese model and selects between JIS and SHIFT-JIS. It is short circuited on non-Japanese models (so GND is on the pin to the chip that makes the selection).

Switch 4 is called CR/CR,LF. It selects the function of the carriage return code (ODH) in the plotter printer text mode. When it is set to:
- ON: the pen position is set to the beginning of the next line.
- OFF: the pen position is set to the beginning of the line.

You may have to set the DIP switches according to your software. Refer to the operating instructions of the software.

### Buttons
The printer has 4 buttons:

| Button | Function |
|---|---|
| Reset | Press to reset the plotter printer into the power on status. This button is useful to reset the plotter printer when the printing operation is interrupted. Refer to the software's manual for how to interrupt the printing operation. |
| Change Color | Press this button to turn the pen holder and change the printed out characters color. |
| Feed | Press this button to feed the paper. |
| Feed Back | Press this button to feed the paper back. |


## Examples from Sony PRN-C41 manual

### All Characters
```
300 'All Character Font
310 LPRINT
320 LPRINT CHR$(27);"#"
330 LPRINT "S1": LPRINT "A"
340 K=0
350 FOR J=64 TO 95
355 IF (K MOD 16)=0 THEN LPRINT CHR$(27);"C";(K\16) MOD 4
360 LPRINT CHR$(1);CHR$(J);
370 K=K+1: NEXT J
380 FOR J=32 TO 255
385 IF (K MOD 16)=0 THEN LPRINT CHR$(27);"C";(K\16) MOD 4
390 LPRINT CHR$(J);
400 K=K+1: NEXT J
410 LPRINT
```

### Squares
```
100 ' initial Square
105 LPRINT
110 LPRINT CHR$(27);"#"
120 FOR I = 0 TO 3
130 LPRINT "C";I
140 FOR J=1 TO 2
150 LPRINT "J 0,-30,30,0,0,30,-30,0"
160 NEXT J
170 LPRINT "R42,0"
180 NEXT I
190 LPRINT "R 0,-30":LPRINT"C0"
200 LPRINT "A"
```

### Vertical line
```
1000 'Vertical Line
1005 LPRINT
1010 LPRINT CHR$(27);"#"
1020 LPRINT "D0,-100"
1030 LPRINT "A"
```

### Horizontal line
```
2000 'Horizontal Line
2005 LPRINT
2010 LPRINT CHR$(27);"#"
2020 LPRINT "D160,0"
2030 LPRINT "A"
```

### Oblique line
```
3000 'Oblique Line
3005 LPRINT
3010 LPRINT CHR$(27);"#"
3020 LPRINT "D100,-100"
3030 LPRINT "A"
```

### Square and diagonal
```
4000 'Square and Oblique Line
4005 LPRINT
4010 LPRINT CHR$(27);"#"
4020 LPRINT "J 0,-160,160,0,0,160,-160,0, 160,-160"
4030 LPRINT "A"
```

### Circle
```
5000 'Circle
5005 LPRINT
5010 LPRINT CHR$(27);"#"
5020 LPRINT "R125,-125"
5030 LPRINT "I"
5040 LPRINT "M80,0"
5050 P=3.14159
5060 A=1:B=1
5070 FOR R=0 TO 360 STEP 5
5080 S=R/180*P
5090 SI=INT(SIN(A*S)*80)
5100 CO=INT(COS(B*S)*80)
5110 LPRINT "D";CO;",";SI
5120 NEXT
5130 LPRINT "R 0,-125"
5140 LPRINT "A"
```

### Doted line
```
7000 'Doted Line
7005 LPRINT
7020 LPRINT CHR$(27);"#"
7030 LPRINT "C0,L1"
7040 LPRINT "J160,0":LPRINT "P Black"
7050 LPRINT "R-232,-20"
7060 LPRINT "C1,L5"
7070 LPRINT "J160,0":LPRINT "P Blue "
7080 LPRINT "R-232,-20"
7090 LPRINT "C2,L9"
7100 LPRINT "J160,0":LPRINT "P Green"
7110 LPRINT "R-232,-20"
7120 LPRINT "C3,L13"
7130 LPRINT "J160,0":LPRINT "P Red  "
7140 LPRINT "R-232,-20"
7150 LPRINT "C0,L0"
7160 LPRINT "A"
```

### Scale Change
```
8000 'Scale Change"
8005 LPRINT
8010 LPRINT CHR$(27);"#"
8020 FOR I=0 TO 10
8030 LPRINT "S";I:LPRINT"PA"
8040 NEXT I
8050 LPRINT "F"
8060 FOR I=11 TO 15
8070 LPRINT "S";I:LPRINT"PA"
8080 NEXT I
8085 LPRINT "S1"
8090 LPRINT "A"
```

### Rotate characters
```
9000 'Rotate
9005 LPRINT
9010 LPRINT CHR$(27);"#"
9020 LPRINT "S9"
9030 LPRINT "M80,-100"
9040 FOR I=0 TO 3
9050 LPRINT "Q";I
9060 LPRINT "PA"
9070 NEXT I
9075 LPRINT "S1"
9080 LPRINT "A"
```

### Check pattern
```
700 'Check
710 LPRINT
720 LPRINT CHR$(27)+"#"
730 LPRINT "I":C=0
740 FOR I=1 TO 20
750 LPRINT "C";C
760 IF I MOD 5=0 THEN C=C+1
770 IF C>3 THEN C=0
780 LPRINT "J198,0"
790 LPRINT "R0,-5"
800 LPRINT "J-198,0"
810 LPRINT "R0,-5"
820 NEXT
830 LPRINT "R0,5"
840 FOR I=1 TO 20
850 LPRINT "C";C
860 IF I MOD 5=0 THEN C=C+1
870 IF C>3 THEN C=0
880 LPRINT "J0,198"
890 LPRINT "R5,0"
900 LPRINT "J0,-198"
910 LPRINT "R5,-0"
920 NEXT
930 LPRINT "A"
```

### Sinus Curve
```
10 LPRINT:LPRINT CHR$(&H1B)+"#"
20 LPRINT "S1"
30 LPRINT "C2"
40 LPRINT "P*** SIN CURVE ***"
50 LPRINT "F"
60 LPRINT "C0"
70 LPRINT "R10,0"
80 LPRINT "J0,-300"
90 LPRINT "R0,150"
100 LPRINT "I"
110 LPRINT "J450,0"
120 LPRINT "H"
130 LPRINT "C3"
140 FOR J=0 TO 360 STEP 90
150 LPRINT "M";J-15;",";-20
160 LPRINT "P";J
170 NEXT
180 LPRINT "H"
190 LPRINT "C1"
200 FOR X=0 TO 360 STEP 10
210 I=X*3.14/180
220 Y=SIN(I)*100
230 LPRINT "D";X;",";Y
240 NEXT X
250 LPRINT "H"
260 LPRINT "A"
270 END
```

## Daewoo DPL-400 / Talent DPL-400 (not emulated yet)
The command set is almost the same as the Sony PRN-C41(D), but there are some differences. The manual is available, but there is no software that supports this plotter.


## National CF-2311 / Panasonic KX-08P (not emulated yet)
No documentation is available for this plotter at the moment. National did release some software for this plotter, but without the manual it is hard to say how it works.

