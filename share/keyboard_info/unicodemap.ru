#region: Russian
#format: <UNICODE>, <ROW><COL>, <MODIFIERS>
# <UNICODE>: hexadecimal unicode number or DEADKEY<N>
# <ROW>: row in keyboard matrix (hexadecimal: 0-B)
# <COL>: column in keyboard matrix (0-7)
# <MODIFIERS>: space separated list of modifiers:
#              SHIFT CTRL GRAPH CODE
#
# Example characters in comments are encoded in UTF-8
#
00000, 23, CTRL        # ^@
00001, 33, CTRL        # ^A
00002, 22, CTRL        # ^B
00003, 54, CTRL        # ^C
00004, 41, CTRL        # ^D
00005, 51, CTRL        # ^E
00006, 26, CTRL        # ^F
00007, 52, CTRL        # ^G
00008, 75,             # Backspace
00009, 73,             # Tab
0000a, 46, CTRL        # ^J
0000b, 81,             # Home (is Home a unicode character?)
0000c, 40, CTRL        # ^L
0000d, 77,             # Enter/CR
0000e, 56, CTRL        # ^N
0000f, 37, CTRL        # ^O
00010, 34, CTRL        # ^P
00011, 57, CTRL        # ^Q
00012, 82,             # Insert (is Insert a unicode character?)
00013, 30, CTRL        # ^S
00014, 43, CTRL        # ^T
00015, 32, CTRL        # ^U
00016, 17, CTRL        # ^V
00017, 31, CTRL        # ^W
00018, 76,             # Select (is Select a unicode character?)
00019, 50, CTRL        # ^Y
0001a, 45, CTRL        # ^Z
0001b, 72,             # Escape(SDL maps ESC and ^[ to this code)
0001c, 87,             # Right (SDL maps ^\ to this code)
0001d, 84,             # Left  (SDL maps ^] to this code)
0001e, 85,             # Up    (SDL maps ^^ to this code)
0001f, 86,             # Down  (SDL maps ^/ to this code)
00020, 80,             # Space
00021, 02,             # ! (EXCLAMATION MARK)
00022, 03,             # " (QUOTATION MARK)
00023, 04,             # # (NUMBER SIGN)
00024, 12,             # $ (DOLLAR SIGN)
00025, 06,             # % (PERCENT SIGN)
00026, 07,             # & (AMPERSAND)
00027, 10,             # ' (APOSTROPHE)
00028, 11,             # ( (LEFT PARENTHESIS)
00029, 00,             # ) (RIGHT PARENTHESIS)
0002a, 16,             # * (ASTERISK)
0002b, 01,             # + (PLUS SIGN)
0002c, 24, SHIFT       # , (COMMA)
0002d, 14,             # - (HYPHEN-MINUS)
0002e, 21, SHIFT       # . (FULL STOP)
0002f, 25, SHIFT       # / (SOLIDUS)
00030, 12, SHIFT       # 0 (DIGIT ZERO)
00031, 02, SHIFT       # 1 (DIGIT ONE)
00032, 03, SHIFT       # 2 (DIGIT TWO)
00033, 04, SHIFT       # 3 (DIGIT THREE)
00034, 05, SHIFT       # 4 (DIGIT FOUR)
00035, 06, SHIFT       # 5 (DIGIT FIVE)
00036, 07, SHIFT       # 6 (DIGIT SIX)
00037, 10, SHIFT       # 7 (DIGIT SEVEN)
00038, 11, SHIFT       # 8 (DIGIT EIGHT)
00039, 00, SHIFT       # 9 (DIGIT NINE)
0003a, 16, SHIFT       # : (COLON)
0003b, 01, SHIFT       # ; (SEMICOLON)
0003c, 24,             # < (LESS-THAN SIGN)
0003d, 13,             # = (EQUALS SIGN)
0003e, 21,             # > (GREATER-THAN SIGN)
0003f, 25,             # ? (QUESTION MARK)
00040, 23,             # @ (COMMERCIAL AT)
00041, 33, SHIFT       # A (LATIN CAPITAL LETTER A)
00042, 22, SHIFT       # B (LATIN CAPITAL LETTER B)
00043, 54, SHIFT       # C (LATIN CAPITAL LETTER C)
00044, 41, SHIFT       # D (LATIN CAPITAL LETTER D)
00045, 51, SHIFT       # E (LATIN CAPITAL LETTER E)
00046, 26, SHIFT       # F (LATIN CAPITAL LETTER F)
00047, 52, SHIFT       # G (LATIN CAPITAL LETTER G)
00048, 15, SHIFT       # H (LATIN CAPITAL LETTER H)
00049, 27, SHIFT       # I (LATIN CAPITAL LETTER I)
0004a, 46, SHIFT       # J (LATIN CAPITAL LETTER J)
0004b, 47, SHIFT       # K (LATIN CAPITAL LETTER K)
0004c, 40, SHIFT       # L (LATIN CAPITAL LETTER L)
0004d, 53, SHIFT       # M (LATIN CAPITAL LETTER M)
0004e, 56, SHIFT       # N (LATIN CAPITAL LETTER N)
0004f, 37, SHIFT       # O (LATIN CAPITAL LETTER O)
00050, 34, SHIFT       # P (LATIN CAPITAL LETTER P)
00051, 57, SHIFT       # Q (LATIN CAPITAL LETTER Q)
00052, 35, SHIFT       # R (LATIN CAPITAL LETTER R)
00053, 30, SHIFT       # S (LATIN CAPITAL LETTER S)
00054, 43, SHIFT       # T (LATIN CAPITAL LETTER T)
00055, 32, SHIFT       # U (LATIN CAPITAL LETTER U)
00056, 17, SHIFT       # V (LATIN CAPITAL LETTER V)
00057, 31, SHIFT       # W (LATIN CAPITAL LETTER W)
00058, 42, SHIFT       # X (LATIN CAPITAL LETTER X)
00059, 50, SHIFT       # Y (LATIN CAPITAL LETTER Y)
0005a, 45, SHIFT       # Z (LATIN CAPITAL LETTER Z)
0005b, 36,             # [ (LEFT SQUARE BRACKET)
0005c, 20,             # \ (REVERSE SOLIDUS)
0005d, 44,             # ] (RIGHT SQUARE BRACKET)
0005e, 14, SHIFT       # ^ (CIRCUMFLEX ACCENT)
0005f, 13, SHIFT       # _ (LOW LINE)
00060, 25, GRAPH       # ` (GRAVE ACCENT)
00061, 33,             # a (LATIN SMALL LETTER A)
00062, 22,             # b (LATIN SMALL LETTER B)
00063, 54,             # c (LATIN SMALL LETTER C)
00064, 41,             # d (LATIN SMALL LETTER D)
00065, 51,             # e (LATIN SMALL LETTER E)
00066, 26,             # f (LATIN SMALL LETTER F)
00067, 52,             # g (LATIN SMALL LETTER G)
00068, 15,             # h (LATIN SMALL LETTER H)
00069, 27,             # i (LATIN SMALL LETTER I)
0006a, 46,             # j (LATIN SMALL LETTER J)
0006b, 47,             # k (LATIN SMALL LETTER K)
0006c, 40,             # l (LATIN SMALL LETTER L)
0006d, 53,             # m (LATIN SMALL LETTER M)
0006e, 56,             # n (LATIN SMALL LETTER N)
0006f, 37,             # o (LATIN SMALL LETTER O)
00070, 34,             # p (LATIN SMALL LETTER P)
00071, 57,             # q (LATIN SMALL LETTER Q)
00072, 35,             # r (LATIN SMALL LETTER R)
00073, 30,             # s (LATIN SMALL LETTER S)
00074, 43,             # t (LATIN SMALL LETTER T)
00075, 32,             # u (LATIN SMALL LETTER U)
00076, 17,             # v (LATIN SMALL LETTER V)
00077, 31,             # w (LATIN SMALL LETTER W)
00078, 42,             # x (LATIN SMALL LETTER X)
00079, 50,             # y (LATIN SMALL LETTER Y)
0007a, 45,             # z (LATIN SMALL LETTER Z)
0007b, 36, SHIFT       # { (LEFT CURLY BRACKET)
0007c, 55,             # | (VERTICAL LINE)
0007d, 44, SHIFT       # } (RIGHT CURLY BRACKET)
0007e, 55, SHIFT       # ~ (TILDE)
0007f, 83,             # Delete
000a0, 80,             # No-break space (&nbsp;)
000a4, 05,             # ¤ (CURRENCY SIGN)
000b0, 57, SHIFT GRAPH # ° (DEGREE SIGN)
000b1, 13, GRAPH       # ± (PLUS-MINUS SIGN)
000b2, 02, SHIFT GRAPH # ² (SUPERSCRIPT TWO)
000b5, 04, SHIFT GRAPH # µ (MICRO SIGN)
000b7, 30, SHIFT GRAPH # · (MIDDLE DOT)
000df, 03, GRAPH       # ß (LATIN SMALL LETTER SHARP S)
000f7, 24, SHIFT GRAPH # ÷ (DIVISION SIGN)
00393, 05, GRAPH       # Γ (GREEK CAPITAL LETTER GAMMA)
00394, 56, SHIFT GRAPH # Δ (GREEK CAPITAL LETTER DELTA)
00398, 10, SHIFT GRAPH # Θ (GREEK CAPITAL LETTER THETA)
003a3, 30, GRAPH       # Σ (GREEK CAPITAL LETTER SIGMA)
003a6, 07, SHIFT GRAPH # Φ (GREEK CAPITAL LETTER PHI)
003a9, 47, SHIFT GRAPH # Ω (GREEK CAPITAL LETTER OMEGA)
003b1, 01, GRAPH       # α (GREEK SMALL LETTER ALPHA)
003b4, 02, GRAPH       # δ (GREEK SMALL LETTER DELTA)
003c0, 21, GRAPH       # π (GREEK SMALL LETTER PI)
003c3, 01, SHIFT GRAPH # σ (GREEK SMALL LETTER SIGMA)
003c4, 05, SHIFT GRAPH # τ (GREEK SMALL LETTER TAU)
003c9, 23, SHIFT GRAPH # ω (GREEK SMALL LETTER OMEGA)
00410, 33, SHIFT CODE  # А (CYRILLIC CAPITAL LETTER A)
00411, 22, SHIFT CODE  # Б (CYRILLIC CAPITAL LETTER BE)
00412, 31, SHIFT CODE  # В (CYRILLIC CAPITAL LETTER VE)
00413, 52, SHIFT CODE  # Г (CYRILLIC CAPITAL LETTER GHE)
00414, 41, SHIFT CODE  # Д (CYRILLIC CAPITAL LETTER DE)
00415, 51, SHIFT CODE  # Е (CYRILLIC CAPITAL LETTER IE)
00416, 17, SHIFT CODE  # Ж (CYRILLIC CAPITAL LETTER ZHE)
00417, 45, SHIFT CODE  # З (CYRILLIC CAPITAL LETTER ZE)
00418, 27, SHIFT CODE  # И (CYRILLIC CAPITAL LETTER I)
00419, 46, SHIFT CODE  # Й (CYRILLIC CAPITAL LETTER SHORT I)
0041a, 47, SHIFT CODE  # К (CYRILLIC CAPITAL LETTER KA)
0041b, 40, SHIFT CODE  # Л (CYRILLIC CAPITAL LETTER EL)
0041c, 53, SHIFT CODE  # М (CYRILLIC CAPITAL LETTER EM)
0041d, 56, SHIFT CODE  # Н (CYRILLIC CAPITAL LETTER EN)
0041e, 37, SHIFT CODE  # О (CYRILLIC CAPITAL LETTER O)
0041f, 34, SHIFT CODE  # П (CYRILLIC CAPITAL LETTER PE)
00420, 35, SHIFT CODE  # Р (CYRILLIC CAPITAL LETTER ER)
00421, 30, SHIFT CODE  # С (CYRILLIC CAPITAL LETTER ES)
00422, 43, SHIFT CODE  # Т (CYRILLIC CAPITAL LETTER TE)
00423, 32, SHIFT CODE  # У (CYRILLIC CAPITAL LETTER U)
00424, 26, SHIFT CODE  # Ф (CYRILLIC CAPITAL LETTER EF)
00425, 15, SHIFT CODE  # Х (CYRILLIC CAPITAL LETTER HA)
00426, 54, SHIFT CODE  # Ц (CYRILLIC CAPITAL LETTER TSE)
00427, 55, SHIFT CODE  # Ч (CYRILLIC CAPITAL LETTER CHE)
00428, 36, SHIFT CODE  # Ш (CYRILLIC CAPITAL LETTER SHA)
00429, 44, SHIFT CODE  # Щ (CYRILLIC CAPITAL LETTER SHCHA)
0042b, 50, SHIFT CODE  # Ы (CYRILLIC CAPITAL LETTER YERU)
0042c, 42, SHIFT CODE  # Ь (CYRILLIC CAPITAL LETTER SOFT SIGN)
0042d, 20, SHIFT CODE  # Э (CYRILLIC CAPITAL LETTER E)
0042e, 23, SHIFT CODE  # Ю (CYRILLIC CAPITAL LETTER YU)
0042f, 57, SHIFT CODE  # Я (CYRILLIC CAPITAL LETTER YA)
00430, 33, CODE        # а (CYRILLIC SMALL LETTER A)
00431, 22, CODE        # б (CYRILLIC SMALL LETTER BE)
00432, 31, CODE        # в (CYRILLIC SMALL LETTER VE)
00433, 52, CODE        # г (CYRILLIC SMALL LETTER GHE)
00434, 41, CODE        # д (CYRILLIC SMALL LETTER DE)
00435, 51, CODE        # е (CYRILLIC SMALL LETTER IE)
00436, 17, CODE        # ж (CYRILLIC SMALL LETTER ZHE)
00437, 45, CODE        # з (CYRILLIC SMALL LETTER ZE)
00438, 27, CODE        # и (CYRILLIC SMALL LETTER I)
00439, 46, CODE        # й (CYRILLIC SMALL LETTER SHORT I)
0043a, 47, CODE        # к (CYRILLIC SMALL LETTER KA)
0043b, 40, CODE        # л (CYRILLIC SMALL LETTER EL)
0043c, 53, CODE        # м (CYRILLIC SMALL LETTER EM)
0043d, 56, CODE        # н (CYRILLIC SMALL LETTER EN)
0043e, 37, CODE        # о (CYRILLIC SMALL LETTER O)
0043f, 34, CODE        # п (CYRILLIC SMALL LETTER PE)
00440, 35, CODE        # р (CYRILLIC SMALL LETTER ER)
00441, 30, CODE        # с (CYRILLIC SMALL LETTER ES)
00442, 43, CODE        # т (CYRILLIC SMALL LETTER TE)
00443, 32, CODE        # у (CYRILLIC SMALL LETTER U)
00444, 26, CODE        # ф (CYRILLIC SMALL LETTER EF)
00445, 15, CODE        # х (CYRILLIC SMALL LETTER HA)
00446, 54, CODE        # ц (CYRILLIC SMALL LETTER TSE)
00447, 55, CODE        # ч (CYRILLIC SMALL LETTER CHE)
00448, 36, CODE        # ш (CYRILLIC SMALL LETTER SHA)
00449, 44, CODE        # щ (CYRILLIC SMALL LETTER SHCHA)
0044a, 14, CODE        # ъ (CYRILLIC SMALL LETTER HARD SIGN)
0044b, 50, CODE        # ы (CYRILLIC SMALL LETTER YERU)
0044c, 42, CODE        # ь (CYRILLIC SMALL LETTER SOFT SIGN)
0044d, 20, CODE        # э (CYRILLIC SMALL LETTER E)
0044e, 23, CODE        # ю (CYRILLIC SMALL LETTER YU)
0044f, 57, CODE        # я (CYRILLIC SMALL LETTER YA)
02021, 27, SHIFT GRAPH # ‡ (DOUBLE DAGGER)
02022, 11, GRAPH       # • (BULLET)
0207f, 03, SHIFT GRAPH # ⁿ (SUPERSCRIPT LATIN SMALL LETTER N)
02205, 51, SHIFT GRAPH # ∅ (EMPTY SET)
02208, 22, SHIFT GRAPH # ∈ (ELEMENT OF)
02219, 55, SHIFT GRAPH # ∙ (BULLET OPERATOR)
0221a, 07, GRAPH       # √ (SQUARE ROOT)
0221e, 10, GRAPH       # ∞ (INFINITY)
02229, 04, GRAPH       # ∩ (INTERSECTION)
02248, 21, SHIFT GRAPH # ≈ (ALMOST EQUAL TO)
02261, 13, SHIFT GRAPH # ≡ (IDENTICAL TO)
02264, 22, GRAPH       # ≤ (LESS-THAN OR EQUAL TO)
02265, 23, GRAPH       # ≥ (GREATER-THAN OR EQUAL TO)
02302, --,             # ⌂ (HOUSE)
02320, 06, GRAPH       # ⌠ (TOP HALF INTEGRAL)
02321, 06, SHIFT GRAPH # ⌡ (BOTTOM HALF INTEGRAL)
02500, 12, GRAPH       # ─ (BOX DRAWINGS LIGHT HORIZONTAL)
02502, 14, SHIFT GRAPH # │ (BOX DRAWINGS LIGHT VERTICAL)
0250c, 47, GRAPH       # ┌ (BOX DRAWINGS LIGHT DOWN AND RIGHT)
02510, 56, GRAPH       # ┐ (BOX DRAWINGS LIGHT DOWN AND LEFT)
02514, 53, GRAPH       # └ (BOX DRAWINGS LIGHT UP AND RIGHT)
02518, 43, GRAPH       # ┘ (BOX DRAWINGS LIGHT UP AND LEFT)
0251c, 33, GRAPH       # ├ (BOX DRAWINGS LIGHT VERTICAL AND RIGHT)
02524, 35, GRAPH       # ┤ (BOX DRAWINGS LIGHT VERTICAL AND LEFT)
0252c, 51, GRAPH       # ┬ (BOX DRAWINGS LIGHT DOWN AND HORIZONTAL)
02534, 27, GRAPH       # ┴ (BOX DRAWINGS LIGHT UP AND HORIZONTAL)
0253c, 34, GRAPH       # ┼ (BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL)
02571, 24, GRAPH       # ╱ (BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT)
02572, 14, GRAPH       # ╲ (BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT)
02573, 55, GRAPH       # ╳ (BOX DRAWINGS LIGHT DIAGONAL CROSS)
02580, 36, SHIFT GRAPH # ▀ (UPPER HALF BLOCK)
02582, 52, GRAPH       # ▂ (LOWER ONE QUARTER BLOCK)
02584, 36, GRAPH       # ▄ (LOWER HALF BLOCK)
02586, 44, GRAPH       # ▆ (LOWER THREE QUARTERS BLOCK)
02588, 45, GRAPH       # █ (FULL BLOCK)
0258a, 41, GRAPH       # ▊ (LEFT THREE QUARTERS BLOCK)
0258c, 40, GRAPH       # ▌ (LEFT HALF BLOCK)
0258e, 37, GRAPH       # ▎ (LEFT ONE QUARTER BLOCK)
02590, 40, SHIFT GRAPH # ▐ (RIGHT HALF BLOCK)
02596, 35, SHIFT GRAPH # ▖ (QUADRANT LOWER LEFT)
02597, 33, SHIFT GRAPH # ▗ (QUADRANT LOWER RIGHT)
02598, 43, SHIFT GRAPH # ▘ (QUADRANT UPPER LEFT)
0259a, 31, SHIFT GRAPH # ▚ (QUADRANT UPPER LEFT AND LOWER RIGHT)
0259d, 53, SHIFT GRAPH # ▝ (QUADRANT UPPER RIGHT)
0259e, 31, GRAPH       # ▞ (QUADRANT UPPER RIGHT AND LOWER LEFT)
025a0, 26, SHIFT GRAPH # ■ (BLACK SQUARE)
025ac, 26, GRAPH       # ▬ (BLACK RECTANGLE)
025cb, 00, GRAPH       # ○ (WHITE CIRCLE)
025d8, 11, SHIFT GRAPH # ◘ (INVERSE BULLET)
025d9, 00, SHIFT GRAPH # ◙ (INVERSE WHITE CIRCLE)
0263a, 15, GRAPH       # ☺ (WHITE SMILING FACE)
0263b, 15, SHIFT GRAPH # ☻ (BLACK SMILING FACE)
0263c, 57, GRAPH       # ☼ (WHITE SUN WITH RAYS)
02640, 42, SHIFT GRAPH # ♀ (FEMALE SIGN)
02642, 42, GRAPH       # ♂ (MALE SIGN)
02660, 17, GRAPH       # ♠ (BLACK SPADE SUIT)
02663, 20, GRAPH       # ♣ (BLACK CLUB SUIT)
02665, 20, SHIFT GRAPH # ♥ (BLACK HEART SUIT)
02666, 17, SHIFT GRAPH # ♦ (BLACK DIAMOND SUIT)
0266a, 16, GRAPH       # ♪ (EIGHTH NOTE)
0266b, 16, SHIFT GRAPH # ♫ (BEAMED EIGHTH NOTES)
027ca, 34, SHIFT GRAPH # ⟊ (VERTICAL BAR WITH HORIZONTAL STROKE)
1fb6c, 54, GRAPH       # 🭬 (LEFT TRIANGULAR ONE QUARTER BLOCK)
1fb6d, 32, GRAPH       # 🭭 (UPPER TRIANGULAR ONE QUARTER BLOCK)
1fb6e, 54, SHIFT GRAPH # 🭮 (RIGHT TRIANGULAR ONE QUARTER BLOCK)
1fb6f, 32, SHIFT GRAPH # 🭯 (LOWER TRIANGULAR ONE QUARTER BLOCK)
1fb82, 44, SHIFT GRAPH # 🮂 (UPPER ONE QUARTER BLOCK)
1fb85, 52, SHIFT GRAPH # 🮅 (UPPER THREE QUARTERS BLOCK)
1fb87, 41, SHIFT GRAPH # 🮇 (RIGHT ONE QUARTER BLOCK)
1fb8a, 37, SHIFT GRAPH # 🮊 (RIGHT THREE QUARTERS BLOCK)
1fb96, 45, SHIFT GRAPH # 🮖 (INVERSE CHECKER BOARD FILL)
1fb98, 46, GRAPH       # 🮘 (UPPER LEFT TO LOWER RIGHT FILL)
1fb99, 46, SHIFT GRAPH # 🮙 (UPPER RIGHT TO LOWER LEFT FILL)
1fb9a, 50, SHIFT GRAPH # 🮚 (UPPER AND LOWER TRIANGULAR HALF BLOCK)
1fb9b, 50, GRAPH       # 🮛 (LEFT AND RIGHT TRIANGULAR HALF BLOCK)
1fbaf, 12, SHIFT GRAPH # 🮯 (BOX DRAWINGS LIGHT HORIZONTAL WITH VERTICAL STROKE)