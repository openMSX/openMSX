This directory contains mappings of legacy character sets from 8-bit
and 16-bit microcomputers as well as teletext to Unicode. Most of
these mappings are made possible using characters from the new block
U+1FB00-1FBFF Symbols for Legacy Computing.

Some of these systems have two kinds of mappings: a "video" or "memory" mapping
that reflects how bytes in video memory are mapped to characters, and an
"interchange" or "CHR$()" mapping that reflects how keyboard input, the BASIC
CHR$() function, or external I/O maps code values to characters. Either kind of
mapping may be needed depending on the use case.

The following systems are represented here along with notes. All trademarks and
registered trademarks are the property of their respective owners. The company
and product names used here are for identification purposes only.


Coleco Adam

ADAMOS7.TXT: Coleco Adam OS7 character set.
ADAMSWTR.TXT: Coleco Adam SmartWRITER character set.

The OS7 character set includes characters at $05-$1C that compose a graphic
that spells COLECOVISION. These have been mapped to characters in the Private
Use Area.


Amstrad CPC

AMSCPCV.TXT: Amstrad CPC character set as mapped in memory.
AMSCPCI.TXT: Amstrad CPC character set without control characters.

AMSCPCV.TXT includes symbols for control characters at $00-$1F and $7F.
AMSCPCI.TXT does not include these symbols and leaves $00-$1F and $7F
mapped to control characters by default.


Amstrad CP/M Plus, PCW, and ZX Spectrum 3+

AMSCPM.TXT: Amstrad CP/M Plus, PCW, and ZX Spectrum 3+ character set.

The slashed zero at $30 is mapped to U+0030+FE00, the recently-added
standardized variation sequence for a slashed zero. The non-slashed
zero at $7F is mapped to U+0030.


Apple II series

APL2PRIM.TXT: Apple II primary character set as mapped in memory.
APL2ALT1.TXT: Apple II alternate character set, version 1, as mapped in memory.
APL2ALT2.TXT: Apple II alternate character set, version 2, as mapped in memory.
APL2ICHG.TXT: Apple II character set as mapped by CHR$().

Version 1 of the alternate character set has the "running man" characters where
version 2 of the alternate character set has the "inverse return arrow" and
"title bar" characters.

Both versions of the alternate character set include a solid Apple logo at $40
and an open Apple logo at $41. Since the Apple logo is trademarked and thus
cannot be encoded, these are mapped to characters in the Private Use Area.


Mattel Aquarius

AQUARIUS.TXT: Mattel Aquarius character set.

Character $8A can be mapped to either U+2660 BLACK SPADE SUIT or
U+1F6E7 UP-POINTING AIRPLANE. The first mapping is used here.


Atari 8-bit series (ATASCII)

ATARI8VG.TXT: ATASCII graphics character set, memory-mapped.
ATARI8IG.TXT: ATASCII graphics character set, CHR$()-mapped.
ATARI8VI.TXT: ATASCII international character set, memory-mapped.
ATARI8II.TXT: ATASCII international character set, CHR$()-mapped.

The graphics character set has symbols and semigraphics where the international
character set has precomposed Latin characters with diacritics.

The CHR$() mappings are in a different order and do not include the control
characters at $1B-$1F, $7D-$7F, $8B-$8F, and $FD-$FF.


Atari ST

ATARISTV.TXT: Atari ST character set, memory-mapped.
ATARISTI.TXT: Atari ST character set, CHR$()-mapped.

The Atari ST character set is based on and similar to IBM PC code page 437, but
has different characters in some locations, in particular $00-$1F and $B0-$DF.

The CHR$() mapping is identical to the memory mapping except it does not
include the control characters at $00-$1F and $7F.

The Atari ST character set includes an Atari logo at $0E-$0F and an image of
J.R. "Bob" Dobbs at $1C-$1F. Both are trademarked and unsuitable for encoding,
so these are mapped to characters in the Private Use Area.


Bildschirmtext (BTX)

BTXG0.TXT: Bildschirmtext G0 character set.
BTXG1.TXT: Bildschirmtext G1 character set.
BTXG2.TXT: Bildschirmtext G2 character set.
BTXG3.TXT: Bildschirmtext G3 character set.

The Bildschirmtext (BTX) character set is mostly
a permutation of the Teletext character set.


TRS-80 Color Computer

COCOICHG.TXT: TRS-80 Color Computer "Semigraphics 4" set as mapped by CHR$().
COCOSGR4.TXT: TRS-80 Color Computer "Semigraphics 4" set as mapped in memory.
COCOSGR6.TXT: TRS-80 Color Computer "Semigraphics 6" set as mapped in memory.

The Color Computer, despite being branded as a TRS-80, is a fundamentally
different computer. The "Semigraphics 4" mode, which is default, has a
character set organized thusly:

$00 - $1F - light-on-dark ASCII-1983 uppercase
$20 - $3F - light-on-dark ASCII-1983 punctuation
$40 - $5F - dark-on-light ASCII-1983 uppercase
$60 - $7F - dark-on-light ASCII-1983 punctuation
$80 - $FF - 2x2 block graphics in 8 colors

(The 8 colors are, in order: green, yellow, blue, red, buff, cyan, magenta,
and orange.)

The CHR$() function uses this mapping:

Interchange => Video
$00 - $1F => (control characters)
$20 - $3F => $60 - $7F (dark-on-light punctuation)
$40 - $5F => $40 - $5F (dark-on-light uppercase)
$60 - $7F => $00 - $1F (light-on-dark uppercase)
$80 - $FF => $80 - $FF (2x2 block graphics in 8 colors)

The "Semigraphics 6" mode has 2x3 block graphics at $80-$FF in two colors (blue
and red), although in a different order from Teletext and the TRS-80. ($00-$7F
displays as binary vertical line garbage, which is pretty much useless and
unworthy of encoding.)


Commodore PET (PETSCII)

CPETVPRI.TXT: Commodore PET primary character set as mapped in memory.
CPETVALT.TXT: Commodore PET alternate character set as mapped in memory.
CPETIPRI.TXT: Commodore PET primary character set as mapped by CHR$().
CPETIALT.TXT: Commodore PET alternate character set as mapped by CHR$().

The PET has REVERSE SOLIDUS where the VIC-20, C64, and C128 have POUND SIGN.
The PET and VIC-20 have CHECKER BOARD FILL where the C64 and C128 have
INVERSE CHECKER BOARD FILL, and vice-versa.

The primary character set has uppercase letters where the alternate character
set has lowercase letters. The primary character set has semigraphics
characters where the alternate character set has uppercase letters.

The CHR$() function mapping (or "interchange" mapping) maps to the in-memory
mapping (or "video" mapping) as follows:

Interchange => Video
$00 - $1F => (control characters)
$20 - $3F => $20 - $3F
$40 - $5F => $00 - $1F
$60 - $7F => $40 - $5F
$80 - $9F => (control characters)
$A0 - $BF => $60 - $7F
$C0 - $DF => $40 - $5F
$E0 - $FF => $60 - $7F


Commodore VIC-20 (PETSCII)

CVICVPRI.TXT: Commodore VIC-20 primary character set as mapped in memory.
CVICVALT.TXT: Commodore VIC-20 alternate character set as mapped in memory.
CVICIPRI.TXT: Commodore VIC-20 primary character set as mapped by CHR$().
CVICIALT.TXT: Commodore VIC-20 alternate character set as mapped by CHR$().

The same notes that apply to the Commodore PET apply to the Commodore VIC-20.


Commodore 64 and Commodore 128 (PETSCII)

C64VPRI.TXT: Commodore 64 and 128 primary character set as mapped in memory.
C64VALT.TXT: Commodore 64 and 128 alternate character set as mapped in memory.
C64IPRI.TXT: Commodore 64 and 128 primary character set as mapped by CHR$().
C64IALT.TXT: Commodore 64 and 128 alternate character set as mapped by CHR$().

The same notes that apply to the Commodore PET apply to the Commodore 64 and
Commodore 128.


HP 264x

HP264XUC.TXT: HP 264x Roman uppercase character set.
HP264XLC.TXT: HP 264x Roman lowercase character set.
HP264XMA.TXT: HP 264x math character set.
HP264XLN.TXT: HP 264x line drawing character set.
HP264XLG.TXT: HP 264x large text character set.

Each individual character generator ROM in the HP 264x contained 64 characters.
Characters $00-$3F of the uppercase ROM correspond to ASCII characters $20-$5F.
Characters $00-$1F of the lowercase ROM correspond to ASCII characters $00-$1F.
Characters $20-$3F of the lowercase ROM correspond to ASCII characters $60-$7F.
All mapping files use a straight mapping to $00-$3F.


IBM PC (code page 437)

IBMPCVID.TXT: IBM PC code page 437 as mapped in memory.
IBMPCICH.TXT: IBM PC code page 437 without control characters.

IBMPCVID.TXT includes the graphics characters at $00-$1F and $7F that are
instead mapped to control characters in the mapping provided by Microsoft.
IBMPCICH.TXT is identical to the mapping provided by Microsoft but does not
explicitly list the control characters.


Kaypro

KAYPRONV.TXT: Kaypro normal video character set.
KAYPRORV.TXT: Kaypro reverse video character set.

These include the extra characters at $00-$1F and the 2x4 block
mosaic characters at $80-$FF. The reverse video version is included
to provide a full set of 2x4 block mosaic characters.


Minitel

MINITLG0.TXT: Minitel G0/G2 text character set.
MINITLG1.TXT: Minitel G1 graphics character set.

The G0 mapping also includes G2 characters using the SS2 (single shift 2)
control character ($19). The G1 character set is similar to the Teletext G1
character set.


MSX

MSXVID.TXT: MSX international character set as mapped in memory.
MSXICH.TXT: MSX international character set as mapped using control sequences.
MSXVIDAE.TXT: MSX Arabic character set as mapped in memory.
MSXICHAE.TXT: MSX Arabic character set as mapped using control sequences.
MSXVIDAR.TXT: MSX Arabic character set as mapped in memory.
MSXICHAR.TXT: MSX Arabic character set as mapped using control sequences.
MSXVIDBG.TXT: MSX Brazilian character set as mapped in memory.
MSXICHBG.TXT: MSX Brazilian character set as mapped using control sequences.
MSXVIDBH.TXT: MSX Brazilian character set as mapped in memory.
MSXICHBH.TXT: MSX Brazilian character set as mapped using control sequences.
MSXVIDBR.TXT: MSX Brazilian character set as mapped in memory.
MSXICHBR.TXT: MSX Brazilian character set as mapped using control sequences.
MSXVIDJP.TXT: MSX Japanese character set as mapped in memory.
MSXICHJP.TXT: MSX Japanese character set as mapped using control sequences.
MSXVIDKR.TXT: MSX Korean character set as mapped in memory.
MSXICHKR.TXT: MSX Korean character set as mapped using control sequences.
MSXVIDRU.TXT: MSX Russian character set as mapped in memory.
MSXICHRU.TXT: MSX Russian character set as mapped using control sequences.
SVI328.TXT: Spectravideo SVI-328 character set as mapped in memory.

National character sets were taken from the following MSX models:

AE: Al Alamiah AX-170 (Arabic with some European characters)
AR: Bawareth Perfect MSX1, Yamaha AX500 (Arabic with box drawing characters)
BG: Gradiente Expert XP-800 (Brazilian)
BH: Sharp Hotbit HB-8000 1.1 (Brazilian)
BR: Gradiente Expert DDPlus, Sharp Hotbit HB-8000 1.2 (Brazilian)
JP: Panasonic FS-A1GT, Sony HB-F900 (Japanese)
KR: Daewoo CPC-400S (Korean)
RU: Yamaha YIS-503IIIR, Yamaha YIS-805/128R2 (Russian)

The MSX international character set is based on CP437 but has some additional
punctuation and precomposed letters and a different set of box drawing and
block element characters. The Japanese character set is based on JIS X 0201
but also includes hiragana, a handful of kanji, and some symbols and basic
box drawing characters. The Arabic character set encodes multiple forms with
a single character to make the most of the limited code space, so correct
conversion of Arabic text will require use of the Arabic shaping algorithm.

The memory mapping includes the graphics characters at $00-$1F that are
accessed with double-byte sequences starting with $01 in the interchange
mapping. The memory mapping also includes the character at $7F that is
inaccessible without writing to video memory directly.

The character $FF (or sometimes $7F) is used for the cursor and displays as
the reverse video variant of the character stored at a designated location in
RAM. The BIOS keeps the original character under the cursor at this location.
Since this behavior is not interchangeable, it is left unmapped.

The Spectravideo SVI-328 is a predecessor to the MSX standard. However,
it is not MSX-compatible and uses a different character set.


Sharp MZ series (SharpSCII)

MZ80VJPN.TXT: Sharp MZ-80 Japanese character set by "display code".
MZ80VEUR.TXT: Sharp MZ-80 European character set by "display code".
MZ700VJP.TXT: Sharp MZ-700 Japanese primary character set by "display code".
MZ700VJA.TXT: Sharp MZ-700 Japanese alternate character set by "display code".
MZ700VEP.TXT: Sharp MZ-700 European primary character set by "display code".
MZ700VEA.TXT: Sharp MZ-700 European alternate character set by "display code".
MZ700IJP.TXT: Sharp MZ-700 Japanese primary character set by "ASCII code".
MZ700IEU.TXT: Sharp MZ-700 European primary character set by "ASCII code".

The Sharp MZ-80 character sets and Sharp MZ-700 primary character sets are
very similar but differ at certain "display codes" ($40 and $80 in the
Japanese version and $40, $80, $A4, $A5, $BC, $BE, $BF, and $E5 in the
European version).

The mapping between "ASCII codes" and "display codes" is nontrivial and rather
disorganized. For example, although digits and uppercase letters in both
versions are in order in either mapping, katakana in the Japanese version are
in order when sorted by "ASCII code" but scrambled when sorted by "display
code", and lowercase letters in the European version are scrambled when sorted
by "ASCII code" but in order when sorted by "display code". The mapping is very
similar between the Japanese and European versions, but differ at "ASCII codes"
$6C, $6D, $80, $C0, and $C6 (and yet they left the lowercase letters scrambled).

The Sharp MZ-700 actually had 512 characters, with the alternate set made
accessible on a character-by-character basis by setting a bit in the attribute
RAM where the background and foreground color were also stored. In the Japanese
version, the primary set contained katakana where the alternate set contained
hiragana. In the European version, Japanese characters in the primary set were
replaced with additional block elements and box drawing characters, whereas the
entire alternate set was replaced with a rather large assortment of video game
sprites. The alternate character set was not accessible from BASIC.

The characters unique to the Sharp MZ series are placed in a new block
U+1CC00-1CEAF Symbols for Legacy Computing Supplement. Characters $36-$39
in the European alternate character set, which resemble the ghosts from
Pac-Man, are mapped to Private Use Area characters due to copyright and
trademark restrictions.


Tangerine Oric series (OricSCII)

ORICG0.TXT: Tangerine Oric G0 text character set.
ORICG1.TXT: Tangerine Oric G1 graphics character set.

The G1 character set includes user-defined characters at $60-$7F which have
been mapped to the Private Use Area. The user-defined characters at $70-$7F
are normally unusable due to the character generator RAM overlapping the
text screen RAM.


Ohio Scientific Superboard II / Challenger series

OSI.TXT: Ohio Scientific character set.

There are some unusual characters in the $00-$1F range.


RISC OS

RISCOSI.TXT: RISC OS Latin-1 character set.
RISCOSV.TXT: RISC OS Latin-1 character set with RISC OS-specific characters.
RISCOSB.TXT: RISC OS BFont character set.
RISCEFF.TXT: RISC OS Latin-1 character set used by Electronic Font Foundry.

The RISC OS character set is based on ISO Latin-1 with extra characters at
$80-$9F. RISCOSV.TXT includes characters specific to the RISC OS user interface
not included in RISCOSI.TXT. RISCEFF.TXT is similar to RISCOSI.TXT but with
additional characters and a different mapping used by Electronic Font Foundry,
a third-party supplier of RISC OS fonts.

RISCOSB.TXT is a separate encoding not based on ISO Latin-1 called the BFont
encoding. According to the RISC OS Programmer's Reference Manual it was used
in the BBC Master microcomputer and is retained in RISC OS for compatibility.


Robotron Z9001

ROBOTRON.TXT: Robotron Z9001 character set.

There are duplicate pairs of pseudographics characters at $7F/$FF, $B7/$FB,
$D0/$EB, $D3/$F1, $DC/$E5, $DF/$F7, and $E2/$F9 that have been mapped to the
same characters.


Sharp X1

SHARPX1V.TXT: Sharp X1 8-bit character set as mapped in video memory.
SHARPX1I.TXT: Sharp X1 8-bit character set as output by CHR$().

The video mapping includes characters in the range $00-$1F that cannot be
output through the BIOS, only through manipulating video memory directly.
Notably, the character at $7F can be output through the BIOS.


Sinclair QL

SINCLRQL.TXT: Sinclair QL character set.

Several of the Sinclair QL character set's glyphs featured raised small capital
letters. These have been mapped to MODIFIER LETTER SMALL A-F due to the absence
of *MODIFIER LETTER CAPITAL C and *MODIFIER LETTER CAPITAL F.

There is a strange character at $B5 that looks like a small capital letter Q
with a V shape underneath. I've mapped this to a capital letter Q with a
combining caron below as the closest possible match.


Smalltalk

SMLTLK72.TXT: Smalltalk-72 character set.
SMLTLK78.TXT: Smalltalk-78 character set.

Characters unique to Smalltalk are placed in a new block
U+1D380-1D3FF Miscellaneous Mathematical and Technical Symbols.


Teletext

TELTXTG0.TXT: Teletext G0 English alphanumerics character set.
TELTXTG1.TXT: Teletext G1 English graphics character set.
TELTXTG2.TXT: Teletext G2 Latin Supplementary Set.
TELTXTG3.TXT: Teletext G3 Smooth Mosaics and Line Drawing Set.

See European Telecommunication Standard 300 706 for details.


Texas Instruments TI-99/4a

TI994A.TXT: TI-99/4a character set.

The TI-99/4a character set is mostly US-ASCII, except it includes two special
characters at $1E and $1F and user-defined characters at $7F-$9F. $1E is the
cursor character, which is mapped to FULL BLOCK. $1F is a block the color of
the screen border, which, because of how the TI-99/4a's video actually works,
is best mapped to NO-BREAK SPACE. $7F-$9F are mapped to the Private Use Area.


TRS-80 Model I

TRSM1ORG.TXT: TRS-80 Model I, original version, as mapped in memory.
TRSM1REV.TXT: TRS-80 Model I, revised version, as mapped in memory.
TRSM1ICH.TXT: TRS-80 Model I as mapped by CHR$() (same for both revisions).

The original version has an opening quotation mark, lowercase letters without
descenders, and a tilde where the revised version has a pound sign, lowercase
letters with descenders, and a yen sign. The CHR$() mapping does not have these
characters at all; it duplicates the uppercase region of the character set
instead.


TRS-80 Model III and Model 4

TRSM3VIN.TXT: TRS-80 Model III/4 international set as mapped in memory.
TRSM3VJP.TXT: TRS-80 Model III/4 katakana set as mapped in memory.
TRSM3VRV.TXT: TRS-80 Model III/4 reverse video set as mapped in memory.
TRSM3IIN.TXT: TRS-80 Model III/4 international set as mapped by CHR$().
TRSM3IJP.TXT: TRS-80 Model III/4 katakana set as mapped by CHR$().
TRSM3IRV.TXT: TRS-80 Model III/4 reverse video set as mapped by CHR$().

The IN and JP sets both have semigraphics at $80-$BF, but the IN set has
miscellaneous symbols at $C0-$FF whereas the JP set has halfwidth katakana.
The RV set has no semigraphics at all, but instead has reverse-video versions
of $00-$7F at $80-$FF. The CHR$() mapping of each set is nearly identical to
the corresponding memory mapping but has control characters at $00-$1F instead
of graphics characters.

The Model III and Model 4 include an unidentified character at $FB which has
been mapped to a character in the Private Use Area.


TRS-80 Model 4a

TRSM4AVP.TXT: TRS-80 Model 4a primary character set as mapped in memory.
TRSM4AVA.TXT: TRS-80 Model 4a alternate character set as mapped in memory.
TRSM4AVR.TXT: TRS-80 Model 4a reverse video set as mapped in memory.
TRSM4AIP.TXT: TRS-80 Model 4a primary character set as mapped by CHR$().
TRSM4AIA.TXT: TRS-80 Model 4a alternate character set as mapped by CHR$().
TRSM4AIR.TXT: TRS-80 Model 4a reverse video set as mapped by CHR$().

The Model 4 and Model 4a have completely different characters at $00-$1F.
Otherwise, the primary character set of the Model 4a is identical to the
international set of the Model 4; however, the Model 4a does not have a
katakana set and instead has an alternate character set with some Latin-1
characters and a completely different set of miscellaneous symbols.

The Model 4a includes two unidentified characters at $1D and $FB which have
been mapped to characters in the Private Use Area.


Sinclair ZX80, ZX81, and ZX Spectrum

ZX80.TXT: Sinclair ZX80 character set.
ZX81.TXT: Sinclair ZX81 character set.
ZXSPCTRM.TXT: Sinclair ZX Spectrum character set.
ZXDESKTP.TXT: Sinclair ZX Spectrum "Desktop" character set.
ZXFZXPUA.TXT: "FZX" character set with $80-$FF mapped to the Private Use Area.
ZXFZXLT1.TXT: "FZX" character set with $A0-$FF mapped to Latin-1 (ISO 8859-1).
ZXFZXLT5.TXT: "FZX" character set with $A0-$FF mapped to Latin-5 (ISO 8859-9).
ZXFZXKOI.TXT: "FZX" character set with $A0-$FF mapped to KOI-8.
ZXFZXSLT.TXT: "FZX" character set with $80-$FF mapped to CP1252.

The ZX80 and ZX81 character sets are only defined over $00-$3F and $80-$BF.
Other code values are used for control characters or BASIC tokens and have no
mapping to any visible character. The ZX80 and ZX81 have the same printable
characters but in a different order.

The ZX Spectrum character set, unlike the ZX80 and ZX81 character sets, is
based on ASCII. It includes semigraphics at $80-$8F and user-defined characters
at $90-$A4. Characters beyond $A4 are used for BASIC tokens and have no mapping
to any visible character. The user-defined characters have been mapped to the
Private Use Area.

"Desktop" is a word processing program for the ZX Spectrum published in 1991
by Proxima Software. It uses its own font files with precomposed characters at
$80-$9D to support Czech.

"FZX" is a royalty-free font format for the ZX Spectrum released in 2013 by
Andrew Owen and Einar Saukas. The FZX specification supports but does not
define the encoding for characters $80-$FF. Most FZX fonts that include these
characters use Latin-1 (ISO 8859-1), Latin-5 (ISO 8859-9), or KOI-8.
