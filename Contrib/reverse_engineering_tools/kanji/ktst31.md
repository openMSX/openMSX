# I/O port Kanji ROM and I/O port Hangul/Hanja ROM testing and configuration

OpenMSX I/O port Kanji ROM and I/O port Hangul/Hanja ROM configurations should be characterized based on hardware testing or study of the relevant circuits, and then described inside the `<Kanji>`...`</Kanji>` tag of the machine/extension configuration XML. One method involves running the BASIC test programs found adjacent to this document. To classify additional models, run the test program on them and copy the existing configuration whose emulator results match what you see. For configurations that don't match these, or when you have questions about the test, please reach out to the openMSX team!

## Eligible Machines and Cartridge Extensions

Any machine or cartridge extensions with an I/O port Kanji ROM or I/O port Hangul/Hanja ROM implementation exposed via I/O ports 0xD8/0xD9 and/or 0xDA/0xDB should be tested or studied to determine how it behaves, and then configured accordingly so that OpenMSX can emulate it accurately. If you aren't sure whether a machine or extension contains this, you can try running the test there to find out.

## Interpretation of Results

The test suite (as of version 3.1) now consists of a starting program KTST31.ASC that checks to make sure you have enough RAM and gives a list of all the test sections, and then three separate test programs which it runs in sequence:

- KTST31A.ASC: Part A: Kanji I/O Tests (tests and their interpretations are identical to 2.x)
  - 1/9: Kanji Detection
  - 2/9: Interleaving A
  - 3/9: Interleaving B
- KTST31B.ASC: Part B: Kanji I/O Tests (new tests for non-zero upper address bits and low-port readback)
  - 4/9: Upper Bit Flips
  - 5/9: Lower Readbacks
  - 6/9: Read Port Flips
- KTST31C.ASC: Part C: Hanja I/O Tests (new tests for Daewoo CPC-400/CPC-400S Hanja/Hangul ROM)
  - 7/9: Hanja Detection
  - 8/9: Upper Bit Flips
  - 9/9: Port Rotations

There is also an omnibus version CKTST31.ASC intended for ahead-of-time compilation into CKTST31.BIN with the MSX-BACON compiler. This is loaded by the BASIC program CKTST31.BCL. It combines all the tests, and will be run (along with the BACONLDR.BIN library and loader which is MIT-licensed and from the MSX-BACON distribution) in preference to the separate versions if possible. If the first-stage loader is able to use this version, it will be used instead of the separate ones. The cassette conversion KTST31.CAS excludes this version, and gives all remaining files shorter names to conform to MSX cassette file name constraints. Load that version using `RUN"CAS:KTST31"`. If you have bash, dd, wc, perl, python3, sed, sox, zip, openMSX, the Sanyo PHC-70FD2 system ROM set installed where openMSX can find it, the [msx_bacon](https://github.com/hra1129/msx_basic_compiler) compiler with the `INP(`...`)` fix from https://github.com/hra1129/msx_basic_compiler/pull/32 , and the [zma](https://github.com/hra1129/zma) assembler in your `$PATH` you can rebuild the cassette image, the pre-compiled version, the disk image, and the ZIP file by running `rebuild.sh`. The included public domain Python CAS to WAV converter was written by Google Gemini.

In test result descriptions below, `▒` indicates a region of mixed-up character scanlines, appearing more or less as small noisy shapes. `█` indicates a solid white-filled area, indicating 0xFF data. The fonts and character sizes likely won't match what you see in this text rendition, but it's the character identities that are more important. For systems that include Level 2 kanji, it's okay if the last four characters on the level 2 section of the first test screen (shown below using the unrealistic placeholders `🐐🦋🌺🪐`) look different, or are blank or solid-filled - those are from a vendor-specific extension area.

### Part A: Kanji I/O Tests (tests and their interpretations are identical to 2.x)

If you have a Daewoo CPC-400/400S / X-2/s, it is expected that the test screens in this section will include a lot of incomplete / incorrectly rendered character parts, resembling small noisy shapes. You may safely ignore this section if you are testing on those models.

These test screens exercise JIS Level 1 kanji access, JIS level 2 kanji access, and interleaved access to the ports for both levels. There's also testing of optimized address writes, where only part of an address is updated, and test of byte counter and character address sharing/interference between the levels. There's also a small test using the BIOS font which can work as a check to make sure the tiny collection of non-I/O port kanji on a Japanese MSX are present and functioning correctly. These tests do not exercise 12x12-dot kanji rendering, MSX-View kanji ROM's, or other non-I/O port kanji systems (e.g. those loaded from disk.) These also don't correctly use the Daewoo CPC-400/400S I/O port Hangul/Hanja ROM system, which uses a slightly different access method and a different (proprietary/unique) character set. If your BIOS font is not a Japanese one, the BIOS font parts won't display kanji and kana.

1. Level 1+Level 2 kanji ROM, level 2 determined via read: (like Sony HB-F1XV)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：尸囗尸匚囗厂冂🐐🦋🌺🪐
嗅囑啣漾聒僥僉嗟喇嗷喞嘔啻嘛嗄僮
Lv1/Lv2:●劒　　ｃ匚ｏ卅ο凵κ冂
Lv2↔Lv1:劈★　　噬Ⅸ囂㎡嶇＝嵎…
       （Previous test）　⇨　 ★劒
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
Ⅸ匚㎡卅＝凵…冂ｃ噬ｏ囂ο嶇κ嵎
Ⅸ匚㎡卅＝凵…冂ｃ噬ｏ囂ο嶇κ嵎
Ⅸ匚㎡卅＝凵…冂ｃ噬ｏ囂ο嶇κ嵎
Ⅸ匚㎡卅＝凵…冂ｃ噬ｏ囂ο嶇κ嵎
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃ噬ｏ囂ο嶇κ嵎Ⅸ匚㎡卅＝凵…冂
ｃ噬ｏ囂ο嶇κ嵎Ⅸ匚㎡卅＝凵…冂
ｃ噬ｏ囂ο嶇κ嵎Ⅸ匚㎡卅＝凵…冂
ｃ噬ｏ囂ο嶇κ嵎Ⅸ匚㎡卅＝凵…冂







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <io base="0xDA" num="2" type="O"/>
      <io base="0xDB" num="1" type="I"/>
      <level_2_via>read</level_2_via>
```
2. Only Level 1 kanji ROM, but exposed equivalently on Level 1+Level 2 I/O ports: (like Yamaha YIS-805/256)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：　ｘ　Ⅸｘ㌘…裕湧由猶
Ｌｖ２漢字：　ＭＩＳＤＲＡＷＮ！
Lv1/Lv2:●★　　ｃⅨｏ㎡ο＝κ…
Lv2↔Lv1:●★　　ｃⅨｏ㎡ο＝κ…
       （Previous test）　⇨　 ★★
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃⅨｏ㎡ο＝κ…ｃⅨｏ㎡ο＝κ…
ｃⅨｏ㎡ο＝κ…ｃⅨｏ㎡ο＝κ…
ⅨⅨ㎡㎡＝＝……ｃｃｏｏοοκκ
ⅨⅨ㎡㎡＝＝……ｃｃｏｏοοκκ
ⅨⅨ㎡㎡＝＝……ｃｃｏｏοοκκ
ⅨⅨ㎡㎡＝＝……ｃｃｏｏοοκκ
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃｃｏｏοοκκⅨⅨ㎡㎡＝＝……
ｃｃｏｏοοκκⅨⅨ㎡㎡＝＝……
ｃｃｏｏοοκκⅨⅨ㎡㎡＝＝……
ｃｃｏｏοοκκⅨⅨ㎡㎡＝＝……







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <io base="0xDA" num="2" type="O"/> <!-- mirror 1 -->
      <io base="0xDB" num="1" type="I"/> <!-- mirror 1 -->
      <level_2_via>only_level_1</level_2_via>
```
3. Only level 1 kanji, no side-effects from attempting to access level 2 I/O ports: (like Panasonic FS-A1FX)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：███████████
████████████████
Lv1/Lv2:●█　　ｃ█ｏ█ο█κ█
Lv2↔Lv1:█★　　█Ⅸ█㎡█＝█…
       （Previous test）　⇨　 ●█
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <level_2_via>read</level_2_via>
```
4. Level 1+Level 2 kanji ROM, level 2 determined via write: (like Panasonic FS-A1GT)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：尸囗尸匚囗厂冂🐐🦋🌺🪐
嗅囑啣漾聒僥僉嗟喇嗷喞嘔啻嘛嗄僮
Lv1/Lv2:●劒　　ｃ匚ｏ卅ο凵κ冂
Lv2↔Lv1:劈★　　噬Ⅸ囂㎡嶇＝嵎…
       （Previous test）　⇨　 劒劒
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
匚匚卅卅凵凵冂冂ｃｃｏｏοοκκ
匚匚卅卅凵凵冂冂ｃｃｏｏοοκκ
匚匚卅卅凵凵冂冂ｃｃｏｏοοκκ
匚匚卅卅凵凵冂冂ｃｃｏｏοοκκ
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃｃｏｏοοκκ匚匚卅卅凵凵冂冂
ｃｃｏｏοοκκ匚匚卅卅凵凵冂冂
ｃｃｏｏοοκκ匚匚卅卅凵凵冂冂
ｃｃｏｏοοκκ匚匚卅卅凵凵冂冂







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <io base="0xDA" num="2" type="O"/>
      <io base="0xDB" num="1" type="I"/>
      <level_2_via>write</level_2_via>
```
5. Only level 1 kanji, but Level 2 I/O ports also active and with level interlock between writes and subsequent reads; reads from the "other" port than the pair that was last written don't disturb the counter: (like Sanyo PHC-35J)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：███████████
████████████████
Lv1/Lv2:●█　　ｃ█ｏ█ο█κ█
Lv2↔Lv1:█★　　█Ⅸ█㎡█＝█…
       （Previous test）　⇨　 ██
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█ｃ█ｏ█ο█κ█
████████ｃ█ｏ█ο█κ█
████████ｃ█ｏ█ο█κ█
████████ｃ█ｏ█ο█κ█
████████ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████
ｃ█ｏ█ο█κ█████████







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <io base="0xDA" num="2" type="O"/>
      <io base="0xDB" num="1" type="I"/>
      <level_2_via>interlocked_write_read</level_2_via>
```
6. Level 1+Level 2 kanji ROM, with level interlock between writes and subsequent reads; reads from the "other" port than the pair that was last written don't disturb the counter: (like Sanyo PHC-70FD2)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.1
BIOS:月火水木金土日年円時分秒百千万大中小 ひらがな ｶﾀｶﾅ
         　　　　　　　　　　　　　　　　　　 Romaji ♦♣♥♠
Lv1 漢字：月火水木金土日年円時分
秒百千万大中小 ひらがな カタカナ
Ｒｏｍａｊｉ ｶﾀｶﾅ Romaji◆★▼▲
Ｇｒｅｅｔｉｎｇｓ！　ＰＵＴＫＡ
ＮＪＩ“漢字は最高です。”月月日日年年
Lv2 漢字：尸囗尸匚囗厂冂🐐🦋🌺🪐
嗅囑啣漾聒僥僉嗟喇嗷喞嘔啻嘛嗄僮
Lv1/Lv2:●劒　　ｃ匚ｏ卅ο凵κ冂
Lv2↔Lv1:劈★　　噬Ⅸ囂㎡嶇＝嵎…
       （Previous test）　⇨　 █劒
 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 2/9: Interleaving A  v3.1
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
ｃ匚ｏ卅ο凵κ冂ｃ匚ｏ卅ο凵κ冂
█匚█卅█凵█冂ｃ█ｏ█ο█κ█
█匚█卅█凵█冂ｃ█ｏ█ο█κ█
█匚█卅█凵█冂ｃ█ｏ█ο█κ█
█匚█卅█凵█冂ｃ█ｏ█ο█κ█
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 3/9: Interleaving B  v3.1
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂
ｃ█ｏ█ο█κ██匚█卅█凵█冂







 Please share screen pic+model│press a key
```
```xml
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
      <io base="0xDA" num="2" type="O"/>
      <io base="0xDB" num="1" type="I"/>
      <level_2_via>interlocked_write_read</level_2_via>
```
*(yes, that's the same port exposure and `level_2_via` as for PHC-35J/PHC-70FD... I won't be surprised if it's easy to add level 2 kanji to those models too)*

### Part B: Kanji I/O Tests (new tests for non-zero upper address bits and low-port readback)

If you have a Daewoo CPC-400/400S / X-2/s, it is expected that the test screens in this section will include a lot of incomplete / incorrectly rendered character parts, resembling small noisy shapes. You may safely ignore this section if you are testing on those models.

Test screen 4 repeats the non-BIOS part of test screen 1 from part A, except that it tries different bit combinations in the normally-ignored upper bits of the character address parts. Test screen 5 repeats test screen 4, but tries to read back the data using the other readback port which is likely to return 0xFF. Test screen 6 repeats test screen 5, but alternates between the normal (odd) and unexpected (even) readback ports on each scanline.

```
 Kanji I/O Tests 4/9: Upper Bit Flips v3.1
```
*(refer to non-BIOS part of test screen 1 in part A for expected patterns)*
```

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 5/9: Lower Readbacks v3.1
████████████████
████████████████
████████████████
████████████████
████████████████
████████████████
████████████████
████████████████
████████████████
██████████████

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 6/9: Read Port Flips v3.1
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
▒▒▒▒▒▒▒▒▒▒▒▒▒▒

 Please share screen pic+model│press a key
```

### Part C: Hanja I/O Tests (new tests for Daewoo CPC-400/CPC-400S Hanja/Hangul ROM)

If you don't have a Daewoo CPC-400/400S / X-2/s, it is expected that the test screens in this section will include a lot of incomplete / incorrectly rendered character parts, resembling small noisy shapes. You may safely ignore this section if you are testing on models other than those.

This section includes a small test of BIOS font Hangul rendering. If your machine has a non-Korean BIOS, those parts won't display correctly and that is expected.

This test exercises the I/O port Hangul/Hanja ROM of the Daewoo CPC-400/CPC-400S. It displays 16x16-dot characters from this 256KB ROM, and also uses 8x8 characters from the BIOS ROM scan-doubled to 8x16. It does not exercise the higher-quality non-I/O port Hangul font data present in this computer. Test screen 7 uses the font normally. Test screen 8 tests different bit combinations in the normally-ignored upper bits of the character address parts written to the I/O ports. Test screen 9 varies the I/O ports used for writing and reading back the data.

Expected appearance:
```
 Hanja I/O Tests 7/9: Hanja Detection v3.1
BIOS: 한글 자모              ㅎㅏㄴㄱㅡㄹ ㅈㅏㅁㅗ
                    "English/Romaja" ♦♣♥♠
Daewoo CPC-400 X-II Hangul/Hanja
대우 Ⅹ-Ⅱ/s 한글과 한자(漢字):
😊︎ 바보라도 열흘 안에 한글을
　 배울 수 있다는 말이 있죠.ᐟ
ㅎㅏㄴㄱㅡㄹ　ㅈㅏㅁㅗ　♦♣♥♠
“Ｅｎｇｌｉｓｈ／Ｒｏｍａｊａ”
漢字：月火水木金土日年下上大中小
일본어(日本語):ひらがな カタカナ
Ｆｒｏｍ　ｕｐｐｅｒ　１２８Ｋ：
版荷兌浸豊何浸販罷篇鬪片退評八針
 Please share screen pic+model│press a key
```
```
 Hanja I/O Tests 8/9: Upper Bit Flips v3.1
Daewoo CPC-400 X-II Hangul/Hanja
대우 Ⅹ-Ⅱ/s 한글과 한자(漢字):
😊︎ 바보라도 열흘 안에 한글을
　 배울 수 있다는 말이 있죠.ᐟ
ㅎㅏㄴㄱㅡㄹ　ㅈㅏㅁㅗ　♦♣♥♠
“Ｅｎｇｌｉｓｈ／Ｒｏｍａｊａ”
漢字：月火水木金土日年下上大中小
일본어(日本語):ひらがな カタカナ
Ｆｒｏｍ　ｕｐｐｅｒ　１２８Ｋ：
版荷兌浸豊何浸販罷篇鬪片退評八針


 Please share screen pic+model│press a key
```
```
 Hanja I/O Tests 9/9: Port Rotations  v3.1
Daewoo CPC-400 X-II Hangul/Hanja
대█ █-█/s ███ ██(██):
█ █보██ 열█ █에 ██을 █
█ ██ 수 ██는 ██ ███
███ㄱ███ㅈ███　███♠
███ｇ███ｈ███ｍ███”
███月███金███下███小
███(███):██が█ █タ██
█ｒ███ｕ███ｒ███８██
█荷███何███篇███評██


 Please share screen pic+model│press a key
```
```xml
      <type>hangul</type>
      <io base="0xD8" num="2" type="O"/>
      <io base="0xD9" num="1" type="I"/>
```
