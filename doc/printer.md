# Printer Emulation Information

This document describes the control codes and escape sequences supported by the emulated printers in openMSX.

## MSX Printer (msx-printer)
The MSX printer emulates a standard MSX dot-matrix printer.

### Control Characters

| Code | Name | Function |
|:---:|:---|:---|
| 01H | SOH | Alternate character prefix. The next character (N) is printed as N & 1FH. |
| 07H | BEL | Audible beep (buzzer). |
| 08H | BS | Backspace. Moves the print head one character space back. |
| 09H | TAB | Horizontal tab. Moves the print head to the next tab stop (every 8 characters). |
| 0AH | LF | Line feed. Moves the paper up one line. |
| 0BH | VT | Vertical tab. Same as LF. |
| 0CH | FF | Form feed. Advances the paper to the next page. |
| 0DH | CR | Carriage return. Moves the print head to the left margin. |
| 0EH | SO | Double character-width ON. |
| 0FH | SI | Double character-width OFF. |
| 1BH | ESC | Start of an escape sequence. |

### Escape Sequences

| Sequence | Function |
|:---|:---|
| ESC N | Proportional mode OFF, font density 1.0. |
| ESC E | Proportional mode OFF, font density 1.40 (Elite). |
| ESC Q | Proportional mode OFF, font density 1.72 (Condensed). |
| ESC P | Proportional mode ON, font density 0.90. |
| ESC ! | Letter quality mode ON. |
| ESC " | Letter quality mode OFF. |
| ESC C D | Double strike mode ON. |
| ESC C d | Double strike mode OFF. |
| ESC C I | Italic mode ON. |
| ESC C i | Italic mode OFF. |
| ESC C B | Bold mode ON. |
| ESC C b | Bold mode OFF. |
| ESC C S | Superscript mode ON. |
| ESC C s | Superscript mode OFF. |
| ESC C U | Subscript mode ON. |
| ESC C u | Subscript mode OFF. |
| ESC O S nn | Set skip over perforation to nn lines. |
| ESC L nnn | Set left margin to nnn dots. |
| ESC A | Set line spacing to 1/6 inch. |
| ESC B | Set line spacing to 1/9 inch. |
| ESC T nn | Set line spacing to nn/144 inch. |
| ESC Z nn | Set line spacing to nn/216 inch. |
| ESC @ | Reset printer to default settings. |
| ESC \ nnn | Set right margin to nnn dots. |
| ESC G nnn mmmm | Print bit-image graphics (nnn: density, mmmm: byte count). |
| ESC S nnnn | Print bit-image graphics (nnnn: byte count). |
| ESC X | Underline mode ON. |
| ESC Y | Underline mode OFF. |
| ESC & | Toggle Japanese mode. |
| ESC $ | Toggle Japanese mode. |

---

## Epson Printer (epson-printer)
The Epson printer emulates the Epson FX-80 dot-matrix printer.

### Control Characters

| Code | Name | Function |
|:---:|:---|:---|
| 00H | NUL | Terminates horizontal and vertical TAB setting. |
| 07H | BEL | Sound beeper. |
| 08H | BS | Backspace. |
| 09H | TAB | Horizontal tab. |
| 0AH | LF | Line feed. |
| 0BH | VT | Vertical tab. |
| 0CH | FF | Form feed. |
| 0DH | CR | Carriage return. |
| 0EH | SO | Expanded mode ON (resets after one line). |
| 0FH | SI | Compressed mode ON. |
| 11H | DC1 | Device control 1. |
| 12H | DC2 | Compressed mode OFF. |
| 13H | DC3 | Device control 3. |
| 14H | DC4 | Expanded mode OFF. |
| 18H | CAN | Cancels all text in the print buffer. |
| 1BH | ESC | Start of an escape sequence. |

### Escape Sequences

| Sequence | Function |
|:---|:---|
| ESC ! n | Master Print Mode Select (combined bitmask for elite, compressed, bold, etc.). |
| ESC % n | Select character set (n=0: ROM, n=1: RAM). |
| ESC & ... | Define user-defined characters. |
| ESC * m n1 n2 | Select bit-image graphics mode. |
| ESC - n | Underline mode ON (n=1) or OFF (n=0). |
| ESC 0 | Set line spacing to 1/8 inch. |
| ESC 1 | Set line spacing to 7/72 inch. |
| ESC 2 | Set line spacing to 1/6 inch. |
| ESC 3 n | Set line spacing to n/216 inch. |
| ESC 4 | Italic mode ON. |
| ESC 5 | Italic mode OFF. |
| ESC 6 | Enable printing of international italic characters. |
| ESC 7 | Disable printing of international italic characters. |
| ESC 8 | Enable paper-out sensor. |
| ESC 9 | Disable paper-out sensor. |
| ESC : | Copy ROM character set to RAM. |
| ESC < | Uni-directional printing mode ON. |
| ESC = | Set 8th bit of data to 0. |
| ESC > | Set 8th bit of data to 1. |
| ESC @ | Reset printer. |
| ESC A n | Set line spacing to n/72 inch. |
| ESC E | Emphasized mode (bold) ON. |
| ESC F | Emphasized mode (bold) OFF. |
| ESC G | Double-strike mode ON. |
| ESC H | Double-strike mode OFF. |
| ESC I n | Print characters 1-31 literally (n=1). |
| ESC J n | Immediate line feed of n/216 inch. |
| ESC K n1 n2 | Single-density bit-image graphics (480 dots per line). |
| ESC L n1 n2 | Double-density bit-image graphics (960 dots per line). |
| ESC M | Elite mode ON (12 cpi). |
| ESC P | Elite mode OFF (10 cpi). |
| ESC Q n | Set right margin to n characters. |
| ESC R n | Select international character set (0:USA, 1:FR, 2:DE, 3:UK, 4:DK, 5:SE, 6:IT, 7:ES, 8:JP). |
| ESC S n | Script mode ON (n=0: super, n=1: sub). |
| ESC T | Script mode OFF. |
| ESC U n | Uni-directional mode ON (n=1) or OFF (n=0). |
| ESC W n | Expanded mode ON (n=1) or OFF (n=0). |
| ESC Z n1 n2 | Quadruple-density bit-image graphics. |
| ESC ^ m n1 n2 | Nine-pin bit-image graphics mode. |
| ESC j n | Immediate reverse line feed of n/216 inch. |
| ESC p n | Proportional mode ON (n=1) or OFF (n=0). |

---

## BASIC Examples

### MSX Printer Example
This program demonstrates common formatting codes for the `msx-printer`.

```basic
10 LPRINT "--- MSX Printer Demo ---"
20 LPRINT CHR$(27);"E";"Elite Mode"
30 LPRINT CHR$(27);"Q";"Condensed Mode"
40 LPRINT CHR$(27);"P";"Proportional Mode"
50 LPRINT CHR$(27);"N";"Normal Mode"
60 LPRINT CHR$(27);"!";"Letter Quality ON"
70 LPRINT CHR$(27);CHR$(34);"Letter Quality OFF"
80 LPRINT CHR$(27);"CB";"Bold ON";CHR$(27);"Cb";" OFF"
90 LPRINT CHR$(27);"CI";"Italic ON";CHR$(27);"Ci";" OFF"
100 LPRINT CHR$(27);"X";"Underline ON";CHR$(27);"Y";" OFF"
110 LPRINT "Standard ";CHR$(27);"CS";"Super";CHR$(27);"Cs";" and ";CHR$(27);"CU";"Sub";CHR$(27);"Cu"
120 LPRINT CHR$(14);"Double Width ON"
130 LPRINT CHR$(15);"Double Width OFF"
140 LPRINT CHR$(12); : ' Form Feed
```

### Epson Printer Example
This program demonstrates common formatting codes for the `epson-printer`.

```basic
10 LPRINT "--- Epson FX-80 Demo ---"
20 LPRINT CHR$(27);"M";"Elite Mode (12 cpi)"
30 LPRINT CHR$(27);"P";"Pica Mode (10 cpi)"
40 LPRINT CHR$(15);"Compressed Mode ON";CHR$(18);" OFF"
50 LPRINT CHR$(27);"E";"Emphasized ON";CHR$(27);"F";" OFF"
60 LPRINT CHR$(27);"G";"Double-Strike ON";CHR$(27);"H";" OFF"
70 LPRINT CHR$(27);"4";"Italic Mode ON";CHR$(27);"5";" OFF"
80 LPRINT CHR$(27);"-";CHR$(1);"Underline ON";CHR$(27);"-";CHR$(0);" OFF"
90 LPRINT CHR$(27);"W";CHR$(1);"Expanded ON";CHR$(27);"W";CHR$(0);" OFF"
100 LPRINT "Normal ";CHR$(27);"S";CHR$(0);"Super";CHR$(27);"T";" and ";CHR$(27);"S";CHR$(1);"Sub";CHR$(27);"T"
110 LPRINT CHR$(12); : ' Form Feed
```
