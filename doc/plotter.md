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
The printer has 4 dip switches, which are by factory default all off.

The switch 4 selects the function of the carriage return code (ODH) in the plotter printer text mode. When it is set to: ON: the pen position is set to the beginning of the next line. OFF: the pen position is set to the beginning of the line.

You may have to set the DIP switches according to your software. Refer to the operating instructions of the software.

### Buttons
The printer has 4 buttons:

| Button | Function |
|---|---|
| Reset | Press to reset the plotter printer into the power on status. This button is useful to reset the plotter printer when the printing operation is interrupted. Refer to the software's manual for how to interrupt the printing operation. |
| Change Color | Press this button to turn the pen holder and change the printed out characters color. |
| Feed | Press this button to feed the paper. |
| Feed Back | Press this button to feed the paper back. |


## Examples

### All Characters
```
300 'All Character Font
310 LPRINT
320 LPRINT CHR$(27);"#"
330 LPRINT "S1":LPRINT"C1"
340 LPRINT "A"
350 FOR J=64 TO 95
360 LPRINT CHR$(1);CHR$(J);
370 NEXT J
380 FOR J=32 TO 255
390 LPRINT CHR$(J);
400 NEXT J
410 LPRINT
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


## Daewoo DPL-400 / Talent DPL-400 (not emulated yet)
The command set is almost the same as the Sony PRN-C41(D), but there are some differences. The manual is available, but there is no software that supports this plotter.


## National CF-2311 / Panasonic KX-08P (not emulated yet)
No documentation is available for this plotter at the moment. National did release some software for this plotter, but without the manual it is hard to say how it works.

