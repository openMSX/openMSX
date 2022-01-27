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
00000, 02, CTRL SHIFT  # ^@
00001, 26, CTRL        # ^A
00002, 27, CTRL        # ^B
00003, 30, CTRL        # ^C
00004, 31, CTRL        # ^D
00005, 32, CTRL        # ^E
00006, 33, CTRL        # ^F
00007, 34, CTRL        # ^G
00008, 75,             # Backspace
00009, 73,             # Tab
0000a, 37, CTRL        # ^J
0000b, 81,             # Home (is Home a unicode character?)
0000c, 41, CTRL        # ^L
0000d, 77,             # Enter/CR
0000e, 43, CTRL        # ^N
0000f, 44, CTRL        # ^O
00010, 45, CTRL        # ^P
00011, 46, CTRL        # ^Q
00012, 82,             # Insert (is Insert a unicode character?)
00013, 50, CTRL        # ^S
00014, 51, CTRL        # ^T
00015, 52, CTRL        # ^U
00016, 53, CTRL        # ^V
00017, 54, CTRL        # ^W
00018, 76,             # Select (is Select a unicode character?)
00019, 56, CTRL        # ^Y
0001a, 57, CTRL        # ^Z
0001b, 72,             # Escape(SDL maps ESC and ^[ to this code)
0001c, 87,             # Right (SDL maps ^\ to this code)
0001d, 84,             # Left  (SDL maps ^] to this code)
0001e, 85,             # Up    (SDL maps ^^ to this code)
0001f, 86,             # Down  (SDL maps ^/ to this code)
00020, 80,             # Space
00021, 02,             # !
00022, 03,             # "
00023, 04,             # #
00024, 12,             # $
00025, 06,             # %
00026, 07,             # &
00027, 10,             # '
00028, 11,             # (
00029, 00,             # )
0002a, 16,             # *
0002b, 01,             # +
0002c, 24, SHIFT       # ,
0002d, 14,             # -
0002e, 21, SHIFT       # .
0002f, 25, SHIFT       # /
00030, 12, SHIFT       # 0
00031, 02, SHIFT       # 1
00032, 03, SHIFT       # 2
00033, 04, SHIFT       # 3
00034, 05, SHIFT       # 4
00035, 06, SHIFT       # 5
00036, 07, SHIFT       # 6
00037, 10, SHIFT       # 7
00038, 11, SHIFT       # 8
00039, 00, SHIFT       # 9
0003a, 16, SHIFT       # :
0003b, 01, SHIFT       # ;
0003c, 24,             # <
0003d, 13,             # =
0003e, 21,             # >
0003f, 25,             # ?
00040, 23,             # @
00041, 33, SHIFT       # A
00042, 22, SHIFT       # B
00043, 54, SHIFT       # C
00044, 41, SHIFT       # D
00045, 51, SHIFT       # E
00046, 26, SHIFT       # F
00047, 52, SHIFT       # G
00048, 15, SHIFT       # H
00049, 27, SHIFT       # I
0004a, 46, SHIFT       # J
0004b, 47, SHIFT       # K
0004c, 40, SHIFT       # L
0004d, 53, SHIFT       # M
0004e, 56, SHIFT       # N
0004f, 37, SHIFT       # O
00050, 34, SHIFT       # P
00051, 57, SHIFT       # Q
00052, 35, SHIFT       # R
00053, 30, SHIFT       # S
00054, 43, SHIFT       # T
00055, 32, SHIFT       # U
00056, 17, SHIFT       # V
00057, 31, SHIFT       # W
00058, 42, SHIFT       # X
00059, 50, SHIFT       # Y
0005a, 45, SHIFT       # Z
0005b, 36,             # [
0005c, 20,             # \
0005d, 44,             # ]
0005e, 14, SHIFT       # ^
0005f, 13, SHIFT       # _
00060, 25, GRAPH       # `
00061, 33,             # a
00062, 22,             # b
00063, 54,             # c
00064, 41,             # d
00065, 51,             # e
00066, 26,             # f
00067, 52,             # g
00068, 15,             # h
00069, 27,             # i
0006a, 46,             # j
0006b, 47,             # k
0006c, 40,             # l
0006d, 53,             # m
0006e, 56,             # n
0006f, 37,             # o
00070, 34,             # p
00071, 57,             # q
00072, 35,             # r
00073, 30,             # s
00074, 43,             # t
00075, 32,             # u
00076, 17,             # v
00077, 31,             # w
00078, 42,             # x
00079, 50,             # y
0007a, 45,             # z
0007b, 36, SHIFT       # {
0007c, 55,             # |
0007d, 44, SHIFT       # }
0007e, 55, SHIFT       # ~
0007f, 83,             # Delete
000a0, 80,             # No-break space (&nbsp;)
000a4, 05,             # ¤
000b0, 57, SHIFT GRAPH # °
000b1, 13, GRAPH       # ±
000b2, 02, SHIFT GRAPH # ²
000b7, 30, SHIFT GRAPH # ·
000df, 03, GRAPH       # ß
000f7, 24, SHIFT GRAPH # ÷
00393, 05, GRAPH       # Γ
00394, 56, SHIFT GRAPH # Δ
00395, 22, SHIFT GRAPH # Ε
003a3, 30, GRAPH       # Σ
003a6, 07, SHIFT GRAPH # Φ
003a9, 47, SHIFT GRAPH # Ω
003b1, 01, GRAPH       # α
003b3, 05, SHIFT GRAPH # γ
003b4, 02, GRAPH       # δ
003b8, 10, SHIFT GRAPH # θ
003bc, 04, SHIFT GRAPH # μ
003c0, 21, GRAPH       # π
003c3, 01, SHIFT GRAPH # σ
003c9, 23, SHIFT GRAPH # ω
00410, 33, SHIFT CODE  # А
00411, 22, SHIFT CODE  # Б
00412, 31, SHIFT CODE  # В
00413, 52, SHIFT CODE  # Г
00414, 41, SHIFT CODE  # Д
00415, 51, SHIFT CODE  # Е
00416, 17, SHIFT CODE  # Ж
00417, 45, SHIFT CODE  # З
00418, 27, SHIFT CODE  # И
00419, 46, SHIFT CODE  # Й
0041a, 47, SHIFT CODE  # К
0041b, 40, SHIFT CODE  # Л
0041c, 53, SHIFT CODE  # М
0041d, 56, SHIFT CODE  # Н
0041e, 37, SHIFT CODE  # О
0041f, 34, SHIFT CODE  # П
00420, 35, SHIFT CODE  # Р
00421, 30, SHIFT CODE  # С
00422, 43, SHIFT CODE  # Т
00423, 32, SHIFT CODE  # У
00424, 26, SHIFT CODE  # Ф
00425, 15, SHIFT CODE  # Х
00426, 54, SHIFT CODE  # Ц
00427, 55, SHIFT CODE  # Ч
00428, 36, SHIFT CODE  # Ш
00429, 44, SHIFT CODE  # Щ
0042a, 14, CODE        # Ъ
0042b, 50, SHIFT CODE  # Ы
0042c, 42, SHIFT CODE  # Ь
0042d, 20, SHIFT CODE  # Э
0042e, 23, SHIFT CODE  # Ю
0042f, 57, SHIFT CODE  # Я
00430, 33, CODE        # а
00431, 22, CODE        # б
00432, 31, CODE        # в
00433, 52, CODE        # г
00434, 41, CODE        # д
00435, 51, CODE        # е
00436, 17, CODE        # ж
00437, 45, CODE        # з
00438, 27, CODE        # и
00439, 46, CODE        # й
0043a, 47, CODE        # к
0043b, 40, CODE        # л
0043c, 53, CODE        # м
0043d, 56, CODE        # н
0043e, 37, CODE        # о
0043f, 34, CODE        # п
00440, 35, CODE        # р
00441, 30, CODE        # с
00442, 43, CODE        # т
00443, 32, CODE        # у
00444, 26, CODE        # ф
00445, 15, CODE        # х
00446, 54, CODE        # ц
00447, 55, CODE        # ч
00448, 36, CODE        # ш
00449, 44, CODE        # щ
0044b, 50, CODE        # ы
0044c, 42, CODE        # ь
0044d, 20, CODE        # э
0044e, 23, CODE        # ю
0044f, 57, CODE        # я
02021, 27, SHIFT GRAPH # ‡
02022, 11, GRAPH       # •
0207f, 03, SHIFT GRAPH # ⁿ
02219, 55, SHIFT GRAPH # ∙
0221a, 07, GRAPH       # √
0221e, 10, GRAPH       # ∞
02229, 04, GRAPH       # ∩
02248, 21, SHIFT GRAPH # ≈
02261, 13, SHIFT GRAPH # ≡
02264, 22, GRAPH       # ≤
02265, 23, GRAPH       # ≥
02300, 51, SHIFT GRAPH # ⌀
02320, 06, GRAPH       # ⌠
02321, 06, SHIFT GRAPH # ⌡
02500, 12, GRAPH       # ─
02502, 14, SHIFT GRAPH # │
0250c, 47, GRAPH       # ┌
02510, 56, GRAPH       # ┐
02514, 53, GRAPH       # └
02518, 43, GRAPH       # ┘
0251c, 33, GRAPH       # ├
02524, 35, GRAPH       # ┤
0252c, 51, GRAPH       # ┬
02534, 27, GRAPH       # ┴
0253c, 34, GRAPH       # ┼
02571, 24, GRAPH       # ╱
02572, 14, GRAPH       # ╲
02573, 55, GRAPH       # ╳
02580, 36, SHIFT GRAPH # ▀
02582, 52, GRAPH       # ▂
02584, 36, GRAPH       # ▄
02586, 44, GRAPH       # ▆
02588, 45, GRAPH       # █
02589, 43, SHIFT GRAPH # ▉
0258a, 41, GRAPH       # ▊
0258c, 40, GRAPH       # ▌
0258e, 37, GRAPH       # ▎
02590, 40, SHIFT GRAPH # ▐
02596, 35, SHIFT GRAPH # ▖
02597, 33, SHIFT GRAPH # ▗
0259a, 31, SHIFT GRAPH # ▚
0259d, 53, SHIFT GRAPH # ▝
0259e, 31, GRAPH       # ▞
025a0, 26, SHIFT GRAPH # ■
025a7, 46, GRAPH       # ▧
025ac, 26, GRAPH       # ▬
025cb, 00, GRAPH       # ○
025d8, 11, SHIFT GRAPH # ◘
025d9, 00, SHIFT GRAPH # ◙
0263a, 15, GRAPH       # ☺
0263b, 15, SHIFT GRAPH # ☻
0263c, 57, GRAPH       # ☼
02640, 42, SHIFT GRAPH # ♀
02642, 42, GRAPH       # ♂
02660, 17, GRAPH       # ♠
02663, 20, GRAPH       # ♣
02665, 20, SHIFT GRAPH # ♥
02666, 17, SHIFT GRAPH # ♦
0266a, 16, GRAPH       # ♪
0266b, 16, SHIFT GRAPH # ♫
1fb6c, 54, GRAPH       # LEFT TRIANGULAR ONE QUARTER BLOCK
1fb6d, 32, GRAPH       # UPPER TRIANGULAR ONE QUARTER BLOCK
1fb6e, 54, SHIFT GRAPH # RIGHT TRIANGULAR ONE QUARTER BLOCK
1fb6f, 32, SHIFT GRAPH # LOWER TRIANGULAR ONE QUARTER BLOCK
1fb82, 44, SHIFT GRAPH # UPPER ONE QUARTER BLOCK
1fb87, 41, SHIFT GRAPH # RIGHT ONE QUARTER BLOCK
1fb96, 45, SHIFT GRAPH # INVERSE CHECKER BOARD FILL
1fb99, 46, SHIFT GRAPH # UPPER RIGHT TO LOWER LEFT FILL
1fb9b, 50, GRAPH       # LEFT AND RIGHT TRIANGULAR HALF BLOCK
