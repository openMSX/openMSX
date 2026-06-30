# I/O port Kanji ROM and I/O port Hangul/Hanja ROM testing and configuration

OpenMSX I/O port Kanji ROM and I/O port Hangu/Hanja ROM configurations should be characterized based on hardware testing or study of the relevant circuits, and then described inside the `<Kanji>`...`</Kanji>` tag of the machine/extension configuration XML. One method involves running the BASIC test programs found in Appendix I of this document. To classify additional models, run the test program on them and copy the existing configuration whose emulator results match what you see. For configurations that don't match these, or when you have questions about the test, please reach out to the openMSX team!

## Eligible Machines and Cartridge Extensions

Any machine or cartridge extensions with an I/O port Kanji ROM or I/O port Hangul/Hanja ROM implementation exposed via I/O ports 0xD8/0xD9 and/or 0xDA/0xDB should be tested or studied to determine how it behaves, and then configured accordingly so that OpenMSX can emulate it accurately. If you aren't sure whether a machine or extension contains this, you can try running the test there to find out.

## Interpretation of Results

The test suite (as of version 3.0) now consists of a starting program KTST30.ASC that checks to make sure you have enough RAM and gives a list of all the test sections, and then three separate test programs which it runs in sequence:

KTST30A.ASC: Part A: Kanji I/O Tests (tests and their interpretations are identical to 2.x)
1/9: Kanji Detection
2/9: Interleaving A
3/9: Interleaving B
KTST30B.ASC: Part B: Kanji I/O Tests (new tests for non-zero upper address bits and low-port readback)
4/9: Upper Bit Flips
5/9: Lower Readbacks
6/9: Read Port Flips
KTST30C.ASC: Part C: Hanja I/O Tests (new tests for Daewoo CPC-400/CPC-400S Hanja/Hangul ROM)
7/9: Hanja Detection
8/9: Upper Bit Flips
9/9: Port Rotations
None of these have yet been exercised on real hardware, but that's the next step! I'll need help though, because I don't own a CPC-400S 😅

In test result descriptions below, `▒` indicates a region of mixed-up character scanlines, appearing more or less as small noisy shapes. `█` indicates a solid white-filled area, indicating 0xFF data. The fonts and character sizes likely won't match what you see in this text rendition, but it's the character identities that are more important. For systems that include Level 2 kanji, it's okay if the last four characters on the level 2 section of the first test screen (shown below using the unrealistic placeholders `🐐🦋🌺🪐`) look different, or are blank or solid-filled - those are from a vendor-specific extension area.

### Part A: Kanji I/O Tests (tests and their interpretations are identical to 2.x)

If you have a Daewoo CPC-400/400S / X-2/s, it is expected that the test screens in this section will include a lot of incomplete / incorrectly rendered character parts, resembling small noisy shapes. You may safely ignore this section if you are testing on those models.

These test screens exercise JIS Level 1 kanji access, JIS level 2 kanji access, and interleaved access to the ports for both levels. There's also testing of optimized address writes, where only part of an address is updated, and test of byte counter and character address sharing/interference between the levels. There's also a small test using the BIOS font which can work as a check to make sure the tiny collection of non-I/O port kanji on a Japanese MSX are present and functioning correctly. These tests do not exercise 12x12-dot kanji rendering, MSX-View kanji ROM's, or other non-I/O port kanji systems (e.g. those loaded from disk.) These also don't correctly use the Daewoo CPC-400/400S I/O port Hangul/Hanja ROM system, which uses a slightly different access method and a different (proprietary/unique) character set. If your BIOS font is not a Japanese one, the BIOS font parts won't display kanji and kana.

1. Level 1+Level 2 kanji ROM, level 2 determined via read: (like Sony HB-F1XV)
```
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 1/9: Kanji Detection v3.0
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
 Kanji I/O Tests 2/9: Interleaving A  v3.0
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
 Kanji I/O Tests 3/9: Interleaving B  v3.0
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
 Kanji I/O Tests 4/9: Upper Bit Flips v3.0
```
*(refer to non-BIOS part of test screen 1 in part A for expected patterns)*
```

 Please share screen pic+model│press a key
```
```
 Kanji I/O Tests 5/9: Lower Readbacks v3.0
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
 Kanji I/O Tests 6/9: Read Port Flips v3.0
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
 Hanja I/O Tests 7/9: Hanja Detection v3.0
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
 Hanja I/O Tests 8/9: Upper Bit Flips v3.0
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
 Hanja I/O Tests 9/9: Port Rotations  v3.0
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

## Appendix I: Test Programs

These are version 3.0 of the tests, the latest version at the time of writing. To run these, it is very helpful to activate turbo CPU modes and/or use the MSX BASIC-kun (MSXべーしっ君) / BASIC'n / X-BASIC / NBASIC / `_TURBO` compiler. If you have it built in to your MSX, or installed as a cartridge, the tests can be run at a much more bearable speed.

`KTST30.ASC`:
```basic
1000 DEFINT A-Z:SCREEN0,,0:WIDTH 40:KEY OFF:COLOR 15,1,1:CLS
1010 T$="Kanji/Hanja I/O Tests":V$="v3.0"
1020 PRINT" "T$" "V$" "
1030 PRINT
1040 PRINT"Part A: Kanji I/O Tests"
1050 PRINT" 1/9: Kanji Detection"
1060 PRINT" 2/9: Interleaving A "
1070 PRINT" 3/9: Interleaving B "
1080 PRINT"Part B: Kanji I/O Tests"
1090 PRINT" 4/9: Upper Bit Flips"
1100 PRINT" 5/9: Lower Readbacks"
1110 PRINT" 6/9: Read Port Flips"
1120 PRINT"Part C: Hanja I/O Tests"
1130 PRINT" 7/9: Hanja Detection"
1140 PRINT" 8/9: Upper Bit Flips"
1150 PRINT" 9/9: Port Rotations "
1160 PRINT
1170 PRINT"MSX BASIC-kun: ";:IFFRE(0)<16384 THEN PRINT"Skipped (RAM<32KB)":GOTO 1260
1180 ON ERROR GOTO 1190:B=2:GOTO 1200
1190 RESUME NEXT
1200 _BC
1210 _TURBO ON
1220 B=0
1230 _TURBO OFF
1240 ON ERROR GOTO 0
1250 IF B=0 THEN PRINT"Missing (slower...)":ELSE PRINT "Found (faster!)"
1260 PRINT "CPU Speed: ";:TIME=0:I=0
1270 IF TIME<5 THEN I=I+1:GOTO 1270:ELSE IF I>18 THEN PRINT"Fast":ELSE PRINT"Normal"
1280 PRINT "Memory: ";:IF FRE(0)<8192 THEN PRINT"Insufficient (RAM<16KB)":STOP:ELSE PRINT"OK"
1290 LOCATE 0,23:PRINT"Please wait, loading part A...";:RUN"KTST30A.ASC"
```

`KTST30A.ASC`:
```basic
1000 DEFINT A-Z:A=0:B=2:ON ERROR GOTO 1010:GOTO 1020
1010 RESUME NEXT
1020 _BC
1030 _TURBO ON
1040 B=1
1050 '#I &HAF,&H32,B
1060 _TURBO OFF
1070 ON ERROR GOTO 0:IF FRE(0)<16384 THEN B=0:PRINT" RAM<32KB";
1080 A=0:IF B<>2 THEN GOSUB 1110:GOTO 2100
1090 PRINT" _TURBO";
1100 _TURBO ON(A,B)
1110 DIM L(7),K(7),T(7):BEEP:SCREEN 2:COLOR 15,1,1:CLS:F=0:E=&H2000:'F=(VDP(4)AND&HFC)*&H2000:E=((VDP(3)\128)+(VDP(10)*2))*&H2000
1120 T$="Kanji I/O Tests":V$="v3.0":N$=" Please share screen pic+model"+CHR$(&H16)+"press a key ":Q=7:W=0:C=PEEK(&H4)+&H100*PEEK(&H5)
1130 Z=0:D=0:Y=0:S$=" "+T$+" 1/9: Kanji Detection "+V$+" ":W=&HFF:GOSUB 1840:W=0:D=0
1140 ' 6x8 / 8x8 BIOS
1150 FOR Y=1 TO 2:S$="":IF Y=1 THEN RESTORE 1900:ELSE RESTORE 1930
1160 READ K$:IF K$="" THEN GOTO 1170:ELSE K$="&H"+K$:S$=S$+CHR$(VAL(K$)):GOTO1160
1170 D=0:GOSUB 1840:D=0:NEXT Y
1180 ' 8x16 / 16x16 Kanji
1190 Z=32:RESTORE 1950
1200 READ K$:IF K$=""THEN GOTO 1290ELSE L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1250:ELSE GOSUB 1780
1210 IF Z MOD 32=31 THEN Z=Z+1
1220 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1230 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1850:NEXT J:NEXT X:NEXT Y
1240 Z=Z+2:GOTO 1200
1250 IF K>=&H80 THEN K=K+8*96-&H20
1260 OUT &HD9,(K-&H20)\&H40:OUT &HD8,(K-&H20)MOD&H40
1270 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1870,1850:NEXT J:NEXT X:NEXT Y
1280 Z=Z+1:GOTO 1200
1290 ' v1.0
1300 OUT &HD9,&H2:OUT &HD8,&H3C:OUT &HDB,&H2:OUT &HDA,&H3A
1310 FOR L=1 TO 0 STEP -1:FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2*L+Z MOD 32 TO 2*L+1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1850:NEXT J:NEXT X:NEXT Y:NEXT L
1320 Z=Z+4
1330 GOSUB 1800:IF A=1 THEN GOTO 1750
1340 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 2/9: Interleaving A  "+V$+" ":W=&HFF:GOSUB 1840:W=0:Z=0:D=0
1350 RESTORE 2040:FOR N=0 TO 7:READ K$:L=0:K=VAL("&H"+K$):GOSUB 1780:L(N)=L:K(N)=K:T(N)=T:NEXT N
1360 FOR N=2 TO 7:IF L(0)<>0 OR L(N)<>L(N-2)OR L(N)=L(N-1)OR((K(N)<>K(N-2))AND(T(N)<>T(N-2)))THEN SCREEN 0:PRINT"Data does not conform":A=1:GOTO 1750:ELSE NEXT N
1370 'plan 1/2/3/4
1380 FOR R=0 TO 1:GOSUB 1890
1390 FOR L=0 TO 1:FOR N=0 TO 7:K=K(N):T=T(N):IF L(N)<>L THEN GOTO 1440
1400 IF (R MOD 2=1) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1410 IF N<2 OR K((8+N-2)MOD 8)<>K THEN OUT &HD9+2*L,K
1420 IF (R MOD 2=0) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1430 FOR Y=1+2*((Z+2*N)\32) TO 4+2*((Z+2*N)\32):GOSUB 1770:NEXT Y
1440 NEXT N:NEXT L:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1450 NEXT R
1460 'plan 5/6/7/8
1470 FOR R=0 TO 3:GOSUB 1890
1480 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 5:L=(-(((P>=1)AND(P<=2))OR(P=4)))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1490 IF ((R\2)MOD 2=1) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1500 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1510 IF ((R\2)MOD 2=0) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1520 IF P>=2 THEN FOR Y=1-2*(P>=4)+2*((Z+2*N)\32) TO 2-2*(P>=4)+2*((Z+2*N)\32):GOSUB 1770:NEXT Y
1530 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1540 NEXT R
1550 'plan 9/10/11/12
1560 FOR R=0 TO 3:GOSUB 1890
1570 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+2*2*32-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1580 IF ((R\2)MOD 2=1) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1590 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1600 IF ((R\2)MOD 2=0) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1610 IF P>=4 THEN Y=1+((P-4)\32) MOD 4+2*((Z+2*N)\32):X=((P-4)\16) MOD 2+(Z+2*N) MOD 32:J=((P-4)\2) MOD 8:G=INP(&HD9+2*L):GOSUB 1850
1620 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1630 NEXT R
1640 GOSUB 1800:IF A=1 THEN GOTO 1750
1650 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 3/9: Interleaving B  "+V$+" ":W=&HFF:GOSUB 1840:W=0:Z=0:D=0
1660 'plan 13/14/15/16
1670 FOR R=0 TO 3:GOSUB 1890
1680 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+4-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1690 IF (P<2 AND R<2) OR (P>=2 AND P<4 AND R>=2 AND R<4) AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1700 IF (P<2 AND R>=2 AND R<4) OR (P>=2 AND P<4 AND R<2) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1710 IF P>=4 THEN FOR Y=1-2*(P>=6)+2*((Z+2*N)\32) TO 2-2*(P>=6)+2*((Z+2*N)\32):GOSUB 1770:NEXT Y
1720 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1730 NEXT R
1740 GOSUB 1800:IF A=1 THEN GOTO 1750
1750 IF A=1 THEN SCREEN 0:PRINT"Abnormal exit"
1760 IF B<>2 THEN RETURN:ELSE GOTO 2090
1770 FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1850:NEXT J:NEXT X:RETURN
1780 T=(K MOD &H100)-&H20:K=K\&H100-&H20:IF K>=&H30 THEN K=K-&H30:L=1:ELSE IF K>&H9 AND K<&H30 THEN K=K-&H6:T=T+&H40
1790 K=K*96+T:T=K MOD&H40:K=K\&H40:RETURN
1800 Y=23:S$=N$:W=&HFF:GOSUB 1840:W=0
1810 I$=INKEY$:IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1830:ELSE:IF I$<>"" THEN GOTO 1810:ELSE BEEP
1820 I$=INKEY$:IF I$="" THEN GOTO 1820:ELSE IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1830
1830 RETURN
1840 D=0:FOR X=0 TO LEN(S$)-1:V=ASC(MID$(S$,1+X,1)):Q=7+2*(V>=&H20 AND V<&H7F):FOR J=0 TO 7:G=PEEK(C+8*V+J):GOSUB 1850:NEXT J:D=D+Q-7:NEXT X:Q=7:RETURN
1850 GG=G XOR W:IF GG=0 THEN RETURN:ELSE XX=8*X+D:IF GG=&HFF THEN YY=8*Y+J:LINE(XX,YY)-(XX+Q,YY),15:RETURN:ELSE IF Q=7 AND D MOD 8=0 THEN GOTO 1880:ELSE M=&H80:YY=8*Y+J:FOR I=0 TO Q:IF (GG\M)MOD 2<>0 THEN PSET(XX+I,YY),15
1860 M=M\2:NEXT I:RETURN
1870 RETURN
1880 O=Y*&H100+XX+J:VPOKE E+O,15*&H10:VPOKE F+O,GG:RETURN
1890 IF ((Z+15)MOD 32)<15 THEN Z=((Z\32)+2)*32:RETURN:ELSE RETURN
1900 DATA 42,49,4F,53,3A,01,02,03,04,05,06,07,08,09,0A,0B,0C,0D,0E,0F,1D,1E,1F
1910 DATA 20,EB,F7,96,DE,E5,20,B6,C0,B6,C5
1920 DATA:'End of DATA
1930 DATA 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,FF,52,6F,6D,61,6A,69,FF,83,82,81,80
1940 DATA:'End of DATA
1950 DATA 4C,76,31,20,3441,3B7A,2127,376E,3250,3F65,4C5A,3662,455A,467C,472F,315F,3B7E,4A2C,4943,4934,4069,4B7C,4267,4366,3E2E
1960 DATA 20,2452,2469,242C,244A,20,252B,253F,252B,254A,2352,236F,236D,2361,236A,2369,20,B6,C0,B6,C5,20,52,6F,6D,61,6A,69
1970 DATA 2221,217A,2227,2225
1980 DATA 2347,2372,2365,2365,2374,2369,236E,2367,2373,212A,2121,2350,2355,2354,234B,2341,234E,234A,2349
1990 DATA 2148,3441,3B7A,244F,3A47,3962,2447,2439,2123,2149
2000 DATA 297B,297C,297A
2010 DATA 4C,76,32,20,3441,3B7A,2127,5579,5378,5579,5239,5378,524C,5144,7775,776F,7773,7771
2020 DATA 534C,5376,5332,5F21,665A,5127,5121,534D,5349,5353,5344,5352,5341,5357,534E,512A
2030 DATA 4C,76,31,2F,4C,76,32,3A,217C,517A,2121,2121:'v1.0
2040 DATA 2363,5239,236F,5241,264F,5161,264A,5144:'v2.0
2050 DATA 4C,76,32,E3,4C,76,31,3A,517C,217A,2121,2121
2060 DATA 5363,2239,536F,2241,564F,2161,564A,2144
2070 DATA 20,2277,50,72,65,76,69,6F,75,73,20,74,65,73,74,2278,2121,2327,2121,20
2080 DATA:'End of DATA
2090 _TURBO OFF
2100 IF A=0 THEN SCREEN 0:LOCATE 0,23:PRINT"Please wait, loading part B...";:RUN"KTST30B.ASC"
2110 PRINT:PRINT"If you want to run all tests again:":PRINT:PRINT"  RUN "CHR$(&H22)"KTST30.ASC"CHR$(&H22):PRINT
2120 END
```

`KTST30B.ASC`:
```basic
1000 DEFINT A-Z:A=0:B=2:ON ERROR GOTO 1010:GOTO 1020
1010 RESUME NEXT
1020 _BC
1030 _TURBO ON
1040 B=1
1050 '#I &HAF,&H32,B
1060 _TURBO OFF
1070 ON ERROR GOTO 0:IF FRE(0)<16384 THEN B=0:PRINT" RAM<32KB";
1080 A=0:IF B<>2 THEN GOSUB 1110:GOTO 1940
1090 PRINT" _TURBO";
1100 _TURBO ON(A,B)
1110 DIM L(7),K(7),T(7):BEEP:SCREEN 2:COLOR 15,1,1:CLS:F=0:E=&H2000:'F=(VDP(4)AND&HFC)*&H2000:E=((VDP(3)\128)+(VDP(10)*2))*&H2000
1120 T$="Kanji I/O Tests":V$="v3.0":N$=" Please share screen pic+model"+CHR$(&H16)+"press a key ":Q=7:W=0:C=PEEK(&H4)+&H100*PEEK(&H5)
1130 Z=0:D=0:Y=0:S$=" "+T$+" 4/9: Upper Bit Flips "+V$+" ":W=&HFF:GOSUB 1730:W=0:D=0
1140 ' 8x16 / 16x16 Kanji only cycling upper address bits
1150 ZZ=&HC0:ZY=&HC0:Z=0:RESTORE 1790
1160 READ K$:IF K$=""THEN GOTO 1250ELSE ZZ=(&H40+ZZ)MOD&H100:ZY=(ZY-&H40*(ZZ=0))MOD&H100:L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1210:ELSE GOSUB 1670
1170 IF Z MOD 32=31 THEN Z=Z+1
1180 OUT &HD9+2*L,K+ZY:OUT &HD8+2*L,T+ZZ
1190 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y
1200 Z=Z+2:GOTO 1160
1210 IF K>=&H80 THEN K=K+8*96-&H20
1220 OUT &HD9,(K-&H20)\&H40+ZY:OUT &HD8,(K-&H20)MOD&H40+ZZ
1230 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1760,1740:NEXT J:NEXT X:NEXT Y
1240 Z=Z+1:GOTO 1160
1250 ' v1.0 only varying upper address bits
1260 OUT &HD9,&HC2:OUT &HD8,&HBC:OUT &HDB,&H42:OUT &HDA,&H3A
1270 FOR L=1 TO 0 STEP -1:FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2*L+Z MOD 32 TO 2*L+1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y:NEXT L
1280 Z=Z+4
1290 GOSUB 1690:IF A=1 THEN GOTO 1640
1300 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 5/9: Lower Readbacks "+V$+" ":W=&HFF:GOSUB 1730:W=0:Z=0:D=0
1310 ' 8x16 / 16x16 Kanji only lower-port readbacks
1320 Z=0:RESTORE 1790
1330 READ K$:IF K$=""THEN GOTO 1420ELSE L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1380:ELSE GOSUB 1670
1340 IF Z MOD 32=31 THEN Z=Z+1
1350 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1360 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD8+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y
1370 Z=Z+2:GOTO 1330
1380 IF K>=&H80 THEN K=K+8*96-&H20
1390 OUT &HD9,(K-&H20)\&H40:OUT &HD8,(K-&H20)MOD&H40
1400 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD8+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1760,1740:NEXT J:NEXT X:NEXT Y
1410 Z=Z+1:GOTO 1330
1420 ' v1.0 only lower-port readbacks
1430 OUT &HD9,&H2:OUT &HD8,&H3C:OUT &HDB,&H2:OUT &HDA,&H3A
1440 FOR L=1 TO 0 STEP -1:FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2*L+Z MOD 32 TO 2*L+1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD8+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y:NEXT L
1450 Z=Z+4
1460 GOSUB 1690:IF A=1 THEN GOTO 1640
1470 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 6/9: Read Port Flips "+V$+" ":W=&HFF:GOSUB 1730:W=0:Z=0:D=0
1480 ' 8x16 / 16x16 Kanji only read-port address LSB flips
1490 Z=0:RESTORE 1790
1500 READ K$:IF K$=""THEN GOTO 1590ELSE L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1550:ELSE GOSUB 1670
1510 IF Z MOD 32=31 THEN Z=Z+1
1520 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1530 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9-(J MOD 2)+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y
1540 Z=Z+2:GOTO 1500
1550 IF K>=&H80 THEN K=K+8*96-&H20
1560 OUT &HD9,(K-&H20)\&H40:OUT &HD8,(K-&H20)MOD&H40
1570 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9-(J MOD 2)+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1760,1740:NEXT J:NEXT X:NEXT Y
1580 Z=Z+1:GOTO 1500
1590 ' v1.0 only read-port address LSB flips
1600 OUT &HD9,&H2:OUT &HD8,&H3C:OUT &HDB,&H2:OUT &HDA,&H3A
1610 FOR L=1 TO 0 STEP -1:FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2*L+Z MOD 32 TO 2*L+1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9-(J MOD 2)+2*L):GOSUB 1740:NEXT J:NEXT X:NEXT Y:NEXT L
1620 Z=Z+4
1630 GOSUB 1690
1640 IF A=1 THEN SCREEN 0:PRINT"Abnormal exit"
1650 IF B<>2 THEN RETURN:ELSE GOTO 1930
1660 FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1740:NEXT J:NEXT X:RETURN
1670 T=(K MOD &H100)-&H20:K=K\&H100-&H20:IF K>=&H30 THEN K=K-&H30:L=1:ELSE IF K>&H9 AND K<&H30 THEN K=K-&H6:T=T+&H40
1680 K=K*96+T:T=K MOD&H40:K=K\&H40:RETURN
1690 Y=23:S$=N$:W=&HFF:GOSUB 1730:W=0
1700 I$=INKEY$:IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1720:ELSE:IF I$<>"" THEN GOTO 1700:ELSE BEEP
1710 I$=INKEY$:IF I$="" THEN GOTO 1710:ELSE IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1720
1720 RETURN
1730 D=0:FOR X=0 TO LEN(S$)-1:V=ASC(MID$(S$,1+X,1)):Q=7+2*(V>=&H20 AND V<&H7F):FOR J=0 TO 7:G=PEEK(C+8*V+J):GOSUB 1740:NEXT J:D=D+Q-7:NEXT X:Q=7:RETURN
1740 GG=G XOR W:IF GG=0 THEN RETURN:ELSE XX=8*X+D:IF GG=&HFF THEN YY=8*Y+J:LINE(XX,YY)-(XX+Q,YY),15:RETURN:ELSE IF Q=7 AND D MOD 8=0 THEN GOTO 1770:ELSE M=&H80:YY=8*Y+J:FOR I=0 TO Q:IF (GG\M)MOD 2<>0 THEN PSET(XX+I,YY),15
1750 M=M\2:NEXT I:RETURN
1760 RETURN
1770 O=Y*&H100+XX+J:VPOKE E+O,15*&H10:VPOKE F+O,GG:RETURN
1780 IF ((Z+15)MOD 32)<15 THEN Z=((Z\32)+2)*32:RETURN:ELSE RETURN
1790 DATA 4C,76,31,20,3441,3B7A,2127,376E,3250,3F65,4C5A,3662,455A,467C,472F,315F,3B7E,4A2C,4943,4934,4069,4B7C,4267,4366,3E2E
1800 DATA 20,2452,2469,242C,244A,20,252B,253F,252B,254A,2352,236F,236D,2361,236A,2369,20,B6,C0,B6,C5,20,52,6F,6D,61,6A,69
1810 DATA 2221,217A,2227,2225
1820 DATA 2347,2372,2365,2365,2374,2369,236E,2367,2373,212A,2121,2350,2355,2354,234B,2341,234E,234A,2349
1830 DATA 2148,3441,3B7A,244F,3A47,3962,2447,2439,2123,2149
1840 DATA 297B,297C,297A
1850 DATA 4C,76,32,20,3441,3B7A,2127,5579,5378,5579,5239,5378,524C,5144,7775,776F,7773,7771
1860 DATA 534C,5376,5332,5F21,665A,5127,5121,534D,5349,5353,5344,5352,5341,5357,534E,512A
1870 DATA 4C,76,31,2F,4C,76,32,3A,217C,517A,2121,2121:'v1.0
1880 DATA 2363,5239,236F,5241,264F,5161,264A,5144:'v2.0
1890 DATA 4C,76,32,E3,4C,76,31,3A,517C,217A,2121,2121
1900 DATA 5363,2239,536F,2241,564F,2161,564A,2144
1910 DATA 20,2277,50,72,65,76,69,6F,75,73,20,74,65,73,74,2278,2121,2327,2121,20
1920 DATA:'End of DATA
1930 _TURBO OFF
1940 IF A=0 THEN SCREEN 0:LOCATE 0,23:PRINT"Please wait, loading part C...";:RUN"KTST30C.ASC"
1950 PRINT:PRINT"If you want to run all tests again:":PRINT:PRINT"  RUN "CHR$(&H22)"KTST30.ASC"CHR$(&H22):PRINT
1960 END
```

`KTST30C.ASC`:
```basic
1000 DEFINT A-Z:A=0:B=2:ON ERROR GOTO 1010:GOTO 1020
1010 RESUME NEXT
1020 _BC
1030 _TURBO ON
1040 B=1
1050 '#I &HAF,&H32,B
1060 _TURBO OFF
1070 ON ERROR GOTO 0:IF FRE(0)<16384 THEN B=0:PRINT" RAM<32KB";
1080 A=0:IF B<>2 THEN GOSUB 1110:GOTO 1810
1090 PRINT" _TURBO";
1100 _TURBO ON(A,B)
1110 DIM L(7),K(7),T(7):BEEP:SCREEN 2:COLOR 15,1,1:CLS:F=0:E=&H2000:'F=(VDP(4)AND&HFC)*&H2000:E=((VDP(3)\128)+(VDP(10)*2))*&H2000
1120 T$="Hanja I/O Tests":V$="v3.0":N$=" Please share screen pic+model"+CHR$(&H16)+"press a key ":Q=7:W=0:C=PEEK(&H4)+&H100*PEEK(&H5)
1130 Z=0:D=0:Y=0:S$=" "+T$+" 7/9: Hanja Detection "+V$+" ":W=&HFF:GOSUB 1590:W=0:D=0
1140 ' 6x8 / 8x8 BIOS font
1150 FOR Y=1 TO 2:S$="":IF Y=1 THEN RESTORE 1650:ELSE RESTORE 1670
1160 READ K$:IF K$="" THEN GOTO 1170:ELSE K$="&H"+K$:S$=S$+CHR$(VAL(K$)):GOTO1160
1170 D=0:GOSUB 1590:D=0:NEXT Y
1180 ' 16x16 Hangul+Hanja + 8x8 BIOS font scan-doubled to 8x16
1190 Z=32:RESTORE 1690
1200 READ K$:IF K$=""THEN GOTO 1270ELSE K=VAL("&H"+K$):IFK<&H100 THEN GOTO 1250:ELSE GOSUB 1530
1210 IF Z MOD 32=31 THEN Z=Z+1
1220 OUT &HD9,K:OUT &HD8,T
1230 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR J=0 TO 7:FOR X=Z MOD 32 TO 1+Z MOD 32:G=INP(&HD9):GOSUB 1600:NEXT X:NEXT J:NEXT Y
1240 Z=Z+2:GOTO 1200
1250 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):X=Z MOD 32:FOR J=0 TO 7:G=PEEK(C+8*K+4*((Y-1)MOD 2)+(J\2)):GOSUB 1600:NEXT J:NEXT Y
1260 Z=Z+1:GOTO 1200
1270 GOSUB 1550:IF A=1 THEN GOTO 1500
1280 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 8/9: Upper Bit Flips "+V$+" ":W=&HFF:GOSUB 1590:W=0:Z=0:D=0
1290 ' 16x16 Hangul+Hanja (upper bits flipped) + 8x8 BIOS font scan-doubled to 8x16
1300 ZZ=&HC0:ZY=&H80:Z=0:RESTORE 1690
1310 READ K$:IF K$=""THEN GOTO 1380ELSE ZZ=(&H40+ZZ)MOD&H100:ZY=(ZY-&H80*(ZZ=0))MOD&H100:K=VAL("&H"+K$):IFK<&H100 THEN GOTO 1360:ELSE GOSUB 1530
1320 IF Z MOD 32=31 THEN Z=Z+1
1330 OUT &HD9,K+ZY:OUT &HD8,T+ZZ
1340 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR J=0 TO 7:FOR X=Z MOD 32 TO 1+Z MOD 32:G=INP(&HD9):GOSUB 1600:NEXT X:NEXT J:NEXT Y
1350 Z=Z+2:GOTO 1310
1360 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):X=Z MOD 32:FOR J=0 TO 7:G=PEEK(C+8*K+4*((Y-1)MOD 2)+(J\2)):GOSUB 1600:NEXT J:NEXT Y
1370 Z=Z+1:GOTO 1310
1380 GOSUB 1550:IF A=1 THEN GOTO 1500
1390 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 9/9: Port Rotations  "+V$+" ":W=&HFF:GOSUB 1590:W=0:Z=0:D=0
1400 ' 16x16 Hangul+Hanja (rotating ports) + 8x8 BIOS font scan-doubled to 8x16
1410 L=1:ZZ=1:Z=0:RESTORE 1690
1420 READ K$:IF K$=""THEN GOTO 1490ELSE ZZ=1-ZZ:L=(L-(ZZ=0))MOD 2:K=VAL("&H"+K$):IFK<&H100 THEN GOTO 1470:ELSE GOSUB 1530
1430 IF Z MOD 32=31 THEN Z=Z+1
1440 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1450 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR J=0 TO 7:FOR X=Z MOD 32 TO 1+Z MOD 32:G=INP(&HD9+2*L-ZZ):GOSUB 1600:NEXT X:NEXT J:NEXT Y
1460 Z=Z+2:GOTO 1420
1470 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):X=Z MOD 32:FOR J=0 TO 7:G=PEEK(C+8*K+4*((Y-1)MOD 2)+(J\2)):GOSUB 1600:NEXT J:NEXT Y
1480 Z=Z+1:GOTO 1420
1490 GOSUB 1550:IF A=1 THEN GOTO 1500
1500 IF A=1 THEN SCREEN 0:PRINT"Abnormal exit"
1510 IF B<>2 THEN RETURN:ELSE GOTO 1800
1520 FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1600:NEXT J:NEXT X:RETURN
1530 T=(K MOD &H100)-&H21:K=K\&H100-&H21
1540 K=K*94+T:T=K MOD&H40:K=K\&H40:RETURN
1550 Y=23:S$=N$:W=&HFF:GOSUB 1590:W=0
1560 I$=INKEY$:IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1580:ELSE:IF I$<>"" THEN GOTO 1560:ELSE BEEP
1570 I$=INKEY$:IF I$="" THEN GOTO 1570:ELSE IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:A=1:GOTO 1580
1580 RETURN
1590 D=0:FOR X=0 TO LEN(S$)-1:V=ASC(MID$(S$,1+X,1)):Q=7+2*(V>=&H20 AND V<&H7F):FOR J=0 TO 7:G=PEEK(C+8*V+J):GOSUB 1600:NEXT J:D=D+Q-7:NEXT X:Q=7:RETURN
1600 GG=G XOR W:IF GG=0 THEN RETURN:ELSE XX=8*X+D:IF GG=&HFF THEN YY=8*Y+J:LINE(XX,YY)-(XX+Q,YY),15:RETURN:ELSE IF Q=7 AND D MOD 8=0 THEN GOTO 1630:ELSE M=&H80:YY=8*Y+J:FOR I=0 TO Q:IF (GG\M)MOD 2<>0 THEN PSET(XX+I,YY),15
1610 M=M\2:NEXT I:RETURN
1620 RETURN
1630 O=Y*&H100+XX+J:VPOKE E+O,15*&H10:VPOKE F+O,GG:RETURN
1640 IF ((Z+15)MOD 32)<15 THEN Z=((Z\32)+2)*32:RETURN:ELSE RETURN
1650 DATA 42,49,4F,53,3A,98,99,AB,20,92,99,8C,FC,FD,FC,FD,FC,20,FC,FD,FC,FD,20,98,99,88,86,A5,8B,FF,92,99,8C,A1
1660 DATA:'End of DATA
1670 DATA 20,20,20,20,20,88,FF,8B,20,FE,0C,A1,20,20,20,20,20,20,22,45,6E,67,6C,69,73,68,2F,52,6F,6D,61,6A,61,22,FF,83,82,81,80
1680 DATA:'End of DATA
1690 DATA 44,61,65,77,6F,6F,20,43,50,43,2D,34,30,30,20,58,2D,49,49,20,48,61,6E,67,75,6C,2F,48,61,6E,6A,61
1700 DATA 3267,392D,20,2439,2D,2431,2F,73,20,3D2E,307D,305B,20,3D2E,395C,20,28,4E54,4A2B,29,3A
1710 DATA 2A46,20,3562,362E,3375,327D,20,3869,3D72,20,3834,385E,20,3D2E,307D,394A,20,2121
1720 DATA 2121,20,356F,3930,20,3748,20,3958,3258,3249,20,3476,394F,20,3958,3A2A,2A4E,2121
1730 DATA 235E,2362,2344,2341,237A,2349,2121,2358,2362,2351,236C,2121,215F,2A3E,2A3F,2A40
1740 DATA 2130,2245,226E,2267,226C,2269,2273,2268,222F,2252,226F,226D,2261,226A,2261,2131
1750 DATA 4E54,4A2B,223A,492F,4F45,465C,4336,4044,4D72,497E,4071,4E4A,4547,4139,4B74,4635
1760 DATA 3952,3630,384F,28,497E,4447,476E,29,3A,2552,2569,252C,254A,20,262B,263F,262B,264A
1770 DATA 2246,2272,226F,226D,2220,2275,2270,2270,2265,2272,2220,2231,2232,2238,224B,223A
1780 DATA 4E24,4E4E,4D68,4D56,4E41,4E4B,4D56,4E25,4E21,4E2B,4D7A,4E2A,4D77,4E2F,4E26,4D57
1790 DATA:'End of DATA
1800 _TURBO OFF
1810 IF A=0 THEN SCREEN 0:PRINT"Thank you!"
1820 PRINT:PRINT"If you want to run all tests again:":PRINT:PRINT"  RUN "CHR$(&H22)"KTST30.ASC"CHR$(&H22):PRINT
1830 END
```

If you need to load the tests from cassette rather than from disk, you can use this Bash script to generate a .cas file: (DumpListEditor can convert the resulting .cas file to WAV in case you need that)
```bash
#!/bin/bash --

ls -d ktst30.asc ktst30a.asc ktst30b.asc ktst30c.asc &&
{

 (
     hdr="$(LC_ALL=C printf '\x1f\xa6\xde\xba\xcc\x13\x7d\x74')"
    _cat() {
        local f="$@"
        local sz=$(( $( LC_ALL=C wc -c < "$f" | awk '{print $1}' ) ))
        local i=0
        while [[ $i -lt $sz ]]
        do
            echo "_cat $f $i/$sz" >&2
            dd bs=1 skip=$i count=256 < <(
                LC_ALL=C cat "$f"
                dd bs=1 count=256 < /dev/zero 2>/dev/null |
                    LC_ALL=C tr '\0' $'\x1A'
            ) 2>/dev/null
            i=$((i+256))
            if [[ $i -lt $sz ]]
            then
                LC_ALL=C printf %s "$hdr"
            fi
        done
        LC_ALL=C printf '\xfe\0\0\0\0\0\0\0'
    }
    e="$(
        dd bs=1 count=10 if=/dev/zero 2>/dev/null |
            LC_ALL=C tr '\0' $'\xea'
    )"
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KTST30" "$hdr"
    _cat ktst30.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30A" "$hdr"
    _cat ktst30a.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30B" "$hdr"
    _cat ktst30b.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30C" "$hdr"
    _cat ktst30c.asc
) | LC_ALL=C sed 's,KTST30[.]ASC,CAS:KTST30,g;s,\(KT\)ST\(30[A-Z]\)[.]ASC,CAS:\1\2  ,g' > ktst30.cas

}
```

## Appendix II: Previous Test Program Versions

The more thorough 2.1 test program which informs most of the configurations (2.0 gave equivalent results but didn't run as easily or on as many models) and are also included in the more recent version 3.0 (see Appendix I).
```basic
1000 DEFINT A-Z:B=2:ON ERROR GOTO 1010:GOTO 1020
1010 RESUME NEXT
1020 _BC
1030 _TURBO ON
1040 B=1
1050 '#I &HAF,&H32,B
1060 _TURBO OFF
1070 ON ERROR GOTO 0
1080 IF B<>2 THEN GOTO 1100
1090 _TURBO ON
1100 DIM L(7),K(7),T(7):BEEP:SCREEN 2:COLOR 15,1,1:CLS:F=0:E=&H2000:'F=(VDP(4)AND&HFC)*&H2000:E=((VDP(3)\128)+(VDP(10)*2))*&H2000
1110 T$="Kanji I/O Tests":V$="v2.1":N$=" Please share screen pic+model"+CHR$(&H16)+"press a key ":Q=7:W=0:C=PEEK(&H4)+&H100*PEEK(&H5)
1120 Z=0:D=0:Y=0:S$=" "+T$+" 1/3: Kanji Detection "+V$+" ":W=&HFF:GOSUB 1820:W=0:D=0
1130 ' 6x8 / 8x8 BIOS
1140 FOR Y=1 TO 2:S$="":IF Y=1 THEN RESTORE 1880:ELSE RESTORE 1910
1150 READ K$:IF K$="" THEN GOTO 1160:ELSE K$="&H"+K$:S$=S$+CHR$(VAL(K$)):GOTO1150
1160 D=0:GOSUB 1820:D=0:NEXT Y
1170 ' 8x16 / 16x16 Kanji
1180 Z=32:RESTORE 1930
1190 READ K$:IF K$=""THEN GOTO 1290ELSE L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1240:ELSE GOSUB 1760
1200 IF Z MOD 32=31 THEN Z=Z+1
1210 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1220 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1830:NEXT J:NEXT X:NEXT Y
1230 Z=Z+2:GOTO 1190
1240 IF K>=&H80 THEN K=K+8*96-&H20
1250 OUT &HD9,(K-&H20)\&H40:OUT &HD8,(K-&H20)MOD&H40
1260 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1850,1830:NEXT J:NEXT X:NEXT Y
1270 ' v1.0
1280 Z=Z+1:GOTO 1190
1290 OUT &HD9,&H2:OUT &HD8,&H3C:OUT &HDB,&H2:OUT &HDA,&H3A
1300 FOR L=1 TO 0 STEP -1:FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2*L+Z MOD 32 TO 2*L+1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1830:NEXT J:NEXT X:NEXT Y:NEXT L
1310 Z=Z+4
1320 GOSUB 1780
1330 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 2/3: Interleaving A  "+V$+" ":W=&HFF:GOSUB 1820:W=0:Z=0:D=0
1340 RESTORE 2020:FOR N=0 TO 7:READ K$:L=0:K=VAL("&H"+K$):GOSUB 1760:L(N)=L:K(N)=K:T(N)=T:NEXT N
1350 FOR N=2 TO 7:IF L(0)<>0 OR L(N)<>L(N-2)OR L(N)=L(N-1)OR((K(N)<>K(N-2))AND(T(N)<>T(N-2)))THEN SCREEN 0:PRINT"Data does not conform":END:ELSE NEXT N
1360 'plan 1/2/3/4
1370 FOR R=0 TO 1:GOSUB 1870
1380 FOR L=0 TO 1:FOR N=0 TO 7:K=K(N):T=T(N):IF L(N)<>L THEN GOTO 1430
1390 IF (R MOD 2=1) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1400 IF N<2 OR K((8+N-2)MOD 8)<>K THEN OUT &HD9+2*L,K
1410 IF (R MOD 2=0) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1420 FOR Y=1+2*((Z+2*N)\32) TO 4+2*((Z+2*N)\32):GOSUB 1750:NEXT Y
1430 NEXT N:NEXT L:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1440 NEXT R
1450 'plan 5/6/7/8
1460 FOR R=0 TO 3:GOSUB 1870
1470 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 5:L=(-(((P>=1)AND(P<=2))OR(P=4)))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1480 IF ((R\2)MOD 2=1) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1490 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1500 IF ((R\2)MOD 2=0) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1510 IF P>=2 THEN FOR Y=1-2*(P>=4)+2*((Z+2*N)\32) TO 2-2*(P>=4)+2*((Z+2*N)\32):GOSUB 1750:NEXT Y
1520 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1530 NEXT R
1540 'plan 9/10/11/12
1550 FOR R=0 TO 3:GOSUB 1870
1560 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+2*2*32-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1570 IF ((R\2)MOD 2=1) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1580 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1590 IF ((R\2)MOD 2=0) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1600 IF P>=4 THEN Y=1+((P-4)\32) MOD 4+2*((Z+2*N)\32):X=((P-4)\16) MOD 2+(Z+2*N) MOD 32:J=((P-4)\2) MOD 8:G=INP(&HD9+2*L):GOSUB 1830
1610 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1620 NEXT R
1630 GOSUB 1780
1640 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 3/3: Interleaving B  "+V$+" ":W=&HFF:GOSUB 1820:W=0:Z=0:D=0
1650 'plan 13/14/15/16
1660 FOR R=0 TO 3:GOSUB 1870
1670 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+4-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1680 IF (P<2 AND R<2) OR (P>=2 AND P<4 AND R>=2 AND R<4) AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1690 IF (P<2 AND R>=2 AND R<4) OR (P>=2 AND P<4 AND R<2) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1700 IF P>=4 THEN FOR Y=1-2*(P>=6)+2*((Z+2*N)\32) TO 2-2*(P>=6)+2*((Z+2*N)\32):GOSUB 1750:NEXT Y
1710 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1720 NEXT R
1730 GOSUB 1780
1740 SCREEN 0:PRINT"Thank you!":END
1750 FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1830:NEXT J:NEXT X:RETURN
1760 T=(K MOD &H100)-&H20:K=K\&H100-&H20:IF K>=&H30 THEN K=K-&H30:L=1:ELSE IF K>&H9 AND K<&H30 THEN K=K-&H6:T=T+&H40
1770 K=K*96+T:T=K MOD&H40:K=K\&H40:RETURN
1780 Y=23:S$=N$:W=&HFF:GOSUB 1820:W=0
1790 I$=INKEY$:IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:END:ELSE:IF I$<>"" THEN GOTO 1790:ELSE BEEP
1800 I$=INKEY$:IF I$="" THEN GOTO 1800:ELSE IF I$=CHR$(&H3) OR I$=CHR$(&H1B) THEN SCREEN 0:END
1810 RETURN
1820 D=0:FOR X=0 TO LEN(S$)-1:V=ASC(MID$(S$,1+X,1)):Q=7+2*(V>=&H20 AND V<&H7F):FOR J=0 TO 7:G=PEEK(C+8*V+J):GOSUB 1830:NEXT J:D=D+Q-7:NEXT X:Q=7:RETURN
1830 GG=G XOR W:IF GG=0 THEN RETURN:ELSE XX=8*X+D:IF GG=&HFF THEN YY=8*Y+J:LINE(XX,YY)-(XX+Q,YY),15:RETURN:ELSE IF Q=7 AND D MOD 8=0 THEN GOTO 1860:ELSE M=&H80:YY=8*Y+J:FOR I=0 TO Q:IF (GG\M)MOD 2<>0 THEN PSET(XX+I,YY),15
1840 M=M\2:NEXT I:RETURN
1850 RETURN
1860 O=Y*&H100+XX+J:VPOKE E+O,15*&H10:VPOKE F+O,GG:RETURN
1870 IF ((Z+15)MOD 32)<15 THEN Z=((Z\32)+2)*32:RETURN:ELSE RETURN
1880 DATA 42,49,4F,53,3A,01,02,03,04,05,06,07,08,09,0A,0B,0C,0D,0E,0F,1D,1E,1F
1890 DATA 20,EB,F7,96,DE,E5,20,B6,C0,B6,C5
1900 DATA:'End of DATA
1910 DATA 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,FF,52,6F,6D,61,6A,69,FF,83,82,81,80
1920 DATA:'End of DATA
1930 DATA 4C,76,31,20,3441,3B7A,2127,376E,3250,3F65,4C5A,3662,455A,467C,472F,315F,3B7E,4A2C,4943,4934,4069,4B7C,4267,4366,3E2E
1940 DATA 20,2452,2469,242C,244A,20,252B,253F,252B,254A,2352,236F,236D,2361,236A,2369,20,B6,C0,B6,C5,20,52,6F,6D,61,6A,69
1950 DATA 2221,217A,2227,2225
1960 DATA 2347,2372,2365,2365,2374,2369,236E,2367,2373,212A,2121,2350,2355,2354,234B,2341,234E,234A,2349
1970 DATA 2148,3441,3B7A,244F,3A47,3962,2447,2439,2123,2149
1980 DATA 297B,297C,297A
1990 DATA 4C,76,32,20,3441,3B7A,2127,5579,5378,5579,5239,5378,524C,5144,7775,776F,7773,7771
2000 DATA 534C,5376,5332,5F21,665A,5127,5121,534D,5349,5353,5344,5352,5341,5357,534E,512A
2010 DATA 4C,76,31,2F,4C,76,32,3A,217C,517A,2121,2121:'v1.0
2020 DATA 2363,5239,236F,5241,264F,5161,264A,5144:'v2.0
2030 DATA 4C,76,32,E3,4C,76,31,3A,517C,217A,2121,2121
2040 DATA 5363,2239,536F,2241,564F,2161,564A,2144
2050 DATA 20,2277,50,72,65,76,69,6F,75,73,20,74,65,73,74,2278,2121,2327,2121,20
2060 DATA:'End of DATA
```

The less thorough 1.0 test program the results of which inform the Panasonic FS-A1F configuration:
```basic
1000 'Have BASIC-kun/X-BASIC? Start with _RUN (_BC: first on Sanyo PHC-70FD/2)
1010 DEFINT A-Z:DIM L(7),K(7),T(7):BEEP:SCREEN 2:COLOR 15,1,1:CLS:F=(VDP(4)AND&HFC)*&H2000:E=((VDP(3)\128)+(VDP(10)*2))*&H2000
1020 T$="Kanji I/O Tests":V$="v2.0":N$=" Please share screen pic+model"+CHR$(&H16)+"press a key ":Q=7:W=0:C=PEEK(&H4)+&H100*PEEK(&H5)
1030 Z=0:D=0:Y=0:S$=" "+T$+" 1/3: Kanji Detection "+V$+" ":W=&HFF:GOSUB 1810:W=0:D=0
1040 ' 6x8 / 8x8 BIOS
1050 S$="":RESTORE 1760
1060 READ K$:IF K$="" THEN GOTO 1070:ELSE K$="&H"+K$:S$=S$+CHR$(VAL(K$)):GOTO1060
1070 Y=1:D=0:GOSUB 1810:D=0
1080 S$="":RESTORE 1790
1090 READ K$:IF K$="" THEN GOTO 1100:ELSE K$="&H"+K$:S$=S$+CHR$(VAL(K$)):GOTO1090
1100 Y=2:D=0:GOSUB 1810:D=0
1110 ' 8x16 / 16x16 Kanji
1120 Z=32:RESTORE 1870
1130 READ K$:IF K$=""THEN GOTO 1230ELSE L=0:K=VAL("&H"+K$):IF K<&H100 THEN GOTO 1180:ELSE GOSUB 1700
1140 IF Z MOD 32=31 THEN Z=Z+1
1150 OUT &HD9+2*L,K:OUT &HD8+2*L,T
1160 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1170 Z=Z+2:GOTO 1130
1180 IF K>=&H80 THEN K=K+8*96-&H20
1190 OUT &HD9,(K-&H20)\&H40:OUT &HD8,(K-&H20)MOD&H40
1200 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):ON 1-(X=(Z MOD 32)) GOSUB 1840,1820:NEXT J:NEXT X:NEXT Y
1210 ' v1.0
1220 Z=Z+1:GOTO 1130
1230 OUT &HD9,&H2:OUT &HD8,&H3C:OUT &HDB,&H2:OUT &HDA,&H3A
1240 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=2+Z MOD 32 TO 3+Z MOD 32:FOR J=0 TO 7:G=INP(&HDB):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1250 FOR Y=1+2*(Z\32) TO 2+2*(Z\32):FOR X=Z MOD 32 TO 1+Z MOD 32:FOR J=0 TO 7:G=INP(&HD9):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1260 Z=Z+4
1270 GOSUB 1720
1280 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 2/3: Interleaving A  "+V$+" ":W=&HFF:GOSUB 1810:W=0:Z=0:D=0
1290 RESTORE 1960:FOR N=0 TO 7:READ K$:L=0:K=VAL("&H"+K$):GOSUB 1700:L(N)=L:K(N)=K:T(N)=T:NEXT N
1300 FOR N=2 TO 7:IF L(0)<>0 OR L(N)<>L(N-2)OR L(N)=L(N-1)OR((K(N)<>K(N-2))AND(T(N)<>T(N-2)))THEN SCREEN 0:PRINT"Data does not conform":END:ELSE NEXT N
1310 'plan 1/2/3/4
1320 FOR R=0 TO 1:GOSUB 1860
1330 FOR L=0 TO 1:FOR N=0 TO 7:K=K(N):T=T(N):IF L(N)<>L THEN GOTO 1380
1340 IF (R MOD 2=1) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1350 IF N<2 OR K((8+N-2)MOD 8)<>K THEN OUT &HD9+2*L,K
1360 IF (R MOD 2=0) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1370 FOR Y=1+2*((Z+2*N)\32) TO 4+2*((Z+2*N)\32):FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1380 NEXT N:NEXT L:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1390 NEXT R
1400 'plan 5/6/7/8
1410 FOR R=0 TO 3:GOSUB 1860
1420 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 5:L=(-(((P>=1)AND(P<=2))OR(P=4)))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1430 IF ((R\2)MOD 2=1) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1440 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1450 IF ((R\2)MOD 2=0) AND (P<2 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1460 IF P>=2 THEN FOR Y=1-2*(P>=4)+2*((Z+2*N)\32) TO 2-2*(P>=4)+2*((Z+2*N)\32):FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1470 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1480 NEXT R
1490 'plan 9/10/11/12
1500 FOR R=0 TO 3:GOSUB 1860
1510 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+2*2*32-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1520 IF ((R\2)MOD 2=1) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1530 IF P<2 AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1540 IF ((R\2)MOD 2=0) AND (P>=2 AND P<4 AND (N<2 OR T((8+N-2)MOD 8)<>T)) THEN OUT &HD8+2*L,T
1550 IF P>=4 THEN Y=1+((P-4)\32) MOD 4+2*((Z+2*N)\32):X=((P-4)\16) MOD 2+(Z+2*N) MOD 32:J=((P-4)\2) MOD 8:G=INP(&HD9+2*L):GOSUB 1820
1560 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1570 NEXT R
1580 GOSUB 1720
1590 CLS:Z=0:D=0:Y=0:S$=" "+T$+" 3/3: Interleaving B  "+V$+" ":W=&HFF:GOSUB 1810:W=0:Z=0:D=0
1600 'plan 13/14/15/16
1610 FOR R=0 TO 3:GOSUB 1860
1620 FOR N2=0 TO 7 STEP 2:FOR P=0 TO 4+4-1:L=(P MOD 2)XOR(-(P<4))XOR(R MOD 2):N=N2+L:K=K(N):T=T(N)
1630 IF (P<2 AND R<2) OR (P>=2 AND P<4 AND R>=2 AND R<4) AND (N<2 OR K((8+N-2)MOD 8)<>K) THEN OUT &HD9+2*L,K
1640 IF (P<2 AND R>=2 AND R<4) OR (P>=2 AND P<4 AND R<2) AND (N<2 OR T((8+N-2)MOD 8)<>T) THEN OUT &HD8+2*L,T
1650 IF P>=4 THEN FOR Y=1-2*(P>=6)+2*((Z+2*N)\32) TO 2-2*(P>=6)+2*((Z+2*N)\32):FOR X=(Z+2*N) MOD 32 TO 1+(Z+2*N) MOD 32:FOR J=0 TO 7:G=INP(&HD9+2*L):GOSUB 1820:NEXT J:NEXT X:NEXT Y
1660 NEXT P:NEXT N2:Z=Z+16:IF(Z MOD 32)<16 THEN Z=Z+32
1670 NEXT R
1680 GOSUB 1720
1690 SCREEN 0:PRINT"Thank you!":END
1700 T=(K MOD &H100)-&H20:K=K\&H100-&H20:IF K>=&H30 THEN K=K-&H30:L=1:ELSE IF K>&H9 AND K<&H30 THEN K=K-&H6:T=T+&H40
1710 K=K*96+T:T=K MOD&H40:K=K\&H40:RETURN
1720 Y=23:S$=N$:W=&HFF:GOSUB 1810:W=0
1730 IF INKEY$<>"" THEN GOTO 1730:ELSE BEEP
1740 IF INKEY$="" THEN GOTO 1740
1750 RETURN
1760 DATA 42,49,4F,53,3A,01,02,03,04,05,06,07,08,09,0A,0B,0C,0D,0E,0F,1D,1E,1F
1770 DATA 20,EB,F7,96,DE,E5,20,B6,C0,B6,C5
1780 DATA:'End of DATA
1790 DATA 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,FF,52,6F,6D,61,6A,69,FF,83,82,81,80
1800 DATA:'End of DATA
1810 D=0:FOR X=0 TO LEN(S$)-1:V=ASC(MID$(S$,1+X,1)):Q=7+2*(V>=&H20 AND V<&H7F):FOR J=0 TO 7:G=PEEK(C+8*V+J):GOSUB 1820:NEXT J:D=D+Q-7:NEXT X:Q=7:RETURN
1820 IF (G XOR W)=0 THEN RETURN:ELSE IF (G XOR W)=&HFF THEN LINE(8*X+D,8*Y+J)-(8*X+Q+D,8*Y+J),15:RETURN:ELSE IF Q=7 AND D MOD 8=0 THEN GOTO 1850:ELSE M=&H80:FOR I=0 TO Q:IF ((G XOR W)\M)MOD 2<>0 THEN PSET(8*X+I+D,8*Y+J),15
1830 M=M\2:NEXT I:RETURN
1840 RETURN
1850 O=Y*&H100+X*8+D+J:VPOKE E+O,15*&H10:VPOKE F+O,G XOR W:RETURN
1860 IF ((Z+15)MOD 32)<15 THEN Z=((Z\32)+2)*32:RETURN:ELSE RETURN
1870 DATA 4C,76,31,20,3441,3B7A,2127,376E,3250,3F65,4C5A,3662,455A,467C,472F,315F,3B7E,4A2C,4943,4934,4069,4B7C,4267,4366,3E2E
1880 DATA 20,2452,2469,242C,244A,20,252B,253F,252B,254A,2352,236F,236D,2361,236A,2369,20,B6,C0,B6,C5,20,52,6F,6D,61,6A,69
1890 DATA 2221,217A,2227,2225
1900 DATA 2347,2372,2365,2365,2374,2369,236E,2367,2373,212A,2121,2350,2355,2354,234B,2341,234E,234A,2349
1910 DATA 2148,3441,3B7A,244F,3A47,3962,2447,2439,2123,2149
1920 DATA 297B,297C,297A
1930 DATA 4C,76,32,20,3441,3B7A,2127,5579,5378,5579,5239,5378,524C,5144,7775,776F,7773,7771
1940 DATA 534C,5376,5332,5F21,665A,5127,5121,534D,5349,5353,5344,5352,5341,5357,534E,512A
1950 DATA 4C,76,31,2F,4C,76,32,3A,217C,517A,2121,2121:'v1.0
1960 DATA 2363,5239,236F,5241,264F,5161,264A,5144:'v2.0
1970 DATA 4C,76,32,E3,4C,76,31,3A,517C,217A,2121,2121
1980 DATA 5363,2239,536F,2241,564F,2161,564A,2144
1990 DATA 20,2277,50,72,65,76,69,6F,75,73,20,74,65,73,74,2278,2121,2327,2121,20
2000 DATA:'End of DATA
2010 'Have BASIC-kun/X-BASIC? Start with _RUN (_BC: first on Sanyo PHC-70FD/2)
```
