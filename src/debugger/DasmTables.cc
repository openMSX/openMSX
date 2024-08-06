#include "DasmTables.hh"

namespace openmsx {

/*
 * Letter codes in the instructions:
 *
 *   A - Address
 *   B - Byte value
 *   R - Relative jump offset
 *   W - Word value
 *   X - Indexed address ( 3 byte )
 *   Y - Indexed address ( 4 byte )
 *   I - IX or IY register
 *   ! - Invalid opcode
 *   @ - Invalid opcode
 *   # - Invalid opcode
 */

const std::array<const char*, 256> mnemonic_xx_cb =
{
	"#","#","#","#","#","#","rlc Y"  ,"#",
	"#","#","#","#","#","#","rrc Y"  ,"#",
	"#","#","#","#","#","#","rl Y"   ,"#",
	"#","#","#","#","#","#","rr Y"   ,"#",
	"#","#","#","#","#","#","sla Y"  ,"#",
	"#","#","#","#","#","#","sra Y"  ,"#",
	"#","#","#","#","#","#","sll Y"  ,"#",
	"#","#","#","#","#","#","srl Y"  ,"#",
	"#","#","#","#","#","#","bit 0,Y","#",
	"#","#","#","#","#","#","bit 1,Y","#",
	"#","#","#","#","#","#","bit 2,Y","#",
	"#","#","#","#","#","#","bit 3,Y","#",
	"#","#","#","#","#","#","bit 4,Y","#",
	"#","#","#","#","#","#","bit 5,Y","#",
	"#","#","#","#","#","#","bit 6,Y","#",
	"#","#","#","#","#","#","bit 7,Y","#",
	"#","#","#","#","#","#","res 0,Y","#",
	"#","#","#","#","#","#","res 1,Y","#",
	"#","#","#","#","#","#","res 2,Y","#",
	"#","#","#","#","#","#","res 3,Y","#",
	"#","#","#","#","#","#","res 4,Y","#",
	"#","#","#","#","#","#","res 5,Y","#",
	"#","#","#","#","#","#","res 6,Y","#",
	"#","#","#","#","#","#","res 7,Y","#",
	"#","#","#","#","#","#","set 0,Y","#",
	"#","#","#","#","#","#","set 1,Y","#",
	"#","#","#","#","#","#","set 2,Y","#",
	"#","#","#","#","#","#","set 3,Y","#",
	"#","#","#","#","#","#","set 4,Y","#",
	"#","#","#","#","#","#","set 5,Y","#",
	"#","#","#","#","#","#","set 6,Y","#",
	"#","#","#","#","#","#","set 7,Y","#"
};

const std::array<const char*, 256> mnemonic_cb =
{
	"rlc b"  ,"rlc c"  ,"rlc d"  ,"rlc e"  ,"rlc h"  ,"rlc l"  ,"rlc (hl)"  ,"rlc a"  ,
	"rrc b"  ,"rrc c"  ,"rrc d"  ,"rrc e"  ,"rrc h"  ,"rrc l"  ,"rrc (hl)"  ,"rrc a"  ,
	"rl b"   ,"rl c"   ,"rl d"   ,"rl e"   ,"rl h"   ,"rl l"   ,"rl (hl)"   ,"rl a"   ,
	"rr b"   ,"rr c"   ,"rr d"   ,"rr e"   ,"rr h"   ,"rr l"   ,"rr (hl)"   ,"rr a"   ,
	"sla b"  ,"sla c"  ,"sla d"  ,"sla e"  ,"sla h"  ,"sla l"  ,"sla (hl)"  ,"sla a"  ,
	"sra b"  ,"sra c"  ,"sra d"  ,"sra e"  ,"sra h"  ,"sra l"  ,"sra (hl)"  ,"sra a"  ,
	"sll b"  ,"sll c"  ,"sll d"  ,"sll e"  ,"sll h"  ,"sll l"  ,"sll (hl)"  ,"sll a"  ,
	"srl b"  ,"srl c"  ,"srl d"  ,"srl e"  ,"srl h"  ,"srl l"  ,"srl (hl)"  ,"srl a"  ,
	"bit 0,b","bit 0,c","bit 0,d","bit 0,e","bit 0,h","bit 0,l","bit 0,(hl)","bit 0,a",
	"bit 1,b","bit 1,c","bit 1,d","bit 1,e","bit 1,h","bit 1,l","bit 1,(hl)","bit 1,a",
	"bit 2,b","bit 2,c","bit 2,d","bit 2,e","bit 2,h","bit 2,l","bit 2,(hl)","bit 2,a",
	"bit 3,b","bit 3,c","bit 3,d","bit 3,e","bit 3,h","bit 3,l","bit 3,(hl)","bit 3,a",
	"bit 4,b","bit 4,c","bit 4,d","bit 4,e","bit 4,h","bit 4,l","bit 4,(hl)","bit 4,a",
	"bit 5,b","bit 5,c","bit 5,d","bit 5,e","bit 5,h","bit 5,l","bit 5,(hl)","bit 5,a",
	"bit 6,b","bit 6,c","bit 6,d","bit 6,e","bit 6,h","bit 6,l","bit 6,(hl)","bit 6,a",
	"bit 7,b","bit 7,c","bit 7,d","bit 7,e","bit 7,h","bit 7,l","bit 7,(hl)","bit 7,a",
	"res 0,b","res 0,c","res 0,d","res 0,e","res 0,h","res 0,l","res 0,(hl)","res 0,a",
	"res 1,b","res 1,c","res 1,d","res 1,e","res 1,h","res 1,l","res 1,(hl)","res 1,a",
	"res 2,b","res 2,c","res 2,d","res 2,e","res 2,h","res 2,l","res 2,(hl)","res 2,a",
	"res 3,b","res 3,c","res 3,d","res 3,e","res 3,h","res 3,l","res 3,(hl)","res 3,a",
	"res 4,b","res 4,c","res 4,d","res 4,e","res 4,h","res 4,l","res 4,(hl)","res 4,a",
	"res 5,b","res 5,c","res 5,d","res 5,e","res 5,h","res 5,l","res 5,(hl)","res 5,a",
	"res 6,b","res 6,c","res 6,d","res 6,e","res 6,h","res 6,l","res 6,(hl)","res 6,a",
	"res 7,b","res 7,c","res 7,d","res 7,e","res 7,h","res 7,l","res 7,(hl)","res 7,a",
	"set 0,b","set 0,c","set 0,d","set 0,e","set 0,h","set 0,l","set 0,(hl)","set 0,a",
	"set 1,b","set 1,c","set 1,d","set 1,e","set 1,h","set 1,l","set 1,(hl)","set 1,a",
	"set 2,b","set 2,c","set 2,d","set 2,e","set 2,h","set 2,l","set 2,(hl)","set 2,a",
	"set 3,b","set 3,c","set 3,d","set 3,e","set 3,h","set 3,l","set 3,(hl)","set 3,a",
	"set 4,b","set 4,c","set 4,d","set 4,e","set 4,h","set 4,l","set 4,(hl)","set 4,a",
	"set 5,b","set 5,c","set 5,d","set 5,e","set 5,h","set 5,l","set 5,(hl)","set 5,a",
	"set 6,b","set 6,c","set 6,d","set 6,e","set 6,h","set 6,l","set 6,(hl)","set 6,a",
	"set 7,b","set 7,c","set 7,d","set 7,e","set 7,h","set 7,l","set 7,(hl)","set 7,a"
};

const std::array<const char*, 256> mnemonic_ed =
{
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"in b,(c)","out (c),b","sbc hl,bc","ld (A),bc","neg","retn","im 0","ld i,a",
	"in c,(c)","out (c),c","adc hl,bc","ld bc,(A)","!"  ,"reti","!"   ,"ld r,a",
	"in d,(c)","out (c),d","sbc hl,de","ld (A),de","!"  ,"!"   ,"im 1","ld a,i",
	"in e,(c)","out (c),e","adc hl,de","ld de,(A)","!"  ,"!"   ,"im 2","ld a,r",
	"in h,(c)","out (c),h","sbc hl,hl","ld (A),hl","!"  ,"!"   ,"!"   ,"rrd"   ,
	"in l,(c)","out (c),l","adc hl,hl","ld hl,(A)","!"  ,"!"   ,"!"   ,"rld"   ,
	"in f,(c)","out (c),0","sbc hl,sp","ld (A),sp","!"  ,"!"   ,"!"   ,"!"     ,
	"in a,(c)","out (c),a","adc hl,sp","ld sp,(A)","!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"!"        ,"!"        ,"!"        ,"!"  ,"!"   ,"!"   ,"!"     ,
	"ldi"     ,"cpi"      ,"ini"      ,"outi"     ,"!"  ,"!"   ,"!"   ,"!"     ,
	"ldd"     ,"cpd"      ,"ind"      ,"outd"     ,"!"  ,"!"   ,"!"   ,"!"     ,
	"ldir"    ,"cpir"     ,"inir"     ,"otir"     ,"!"  ,"!"   ,"!"   ,"!"     ,
	"lddr"    ,"cpdr"     ,"indr"     ,"otdr"     ,"!"  ,"!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,b","!"        ,"muluw hl,bc","!","!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,c","!"        ,"!"        ,"!",  "!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,d","!"        ,"muluw hl,de","!","!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,e","!"        ,"!"        ,"!",  "!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,h","!"        ,"muluw hl,hl","!","!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,l","!"        ,"!"        ,"!",  "!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,(hl)","!"     ,"muluw hl,sp","!","!"   ,"!"   ,"!"     ,
	"!"       ,"mulub a,a","!"        ,"!"        ,"!",  "!"   ,"!"   ,"!"
};

const std::array<const char*, 256> mnemonic_xx =
{
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"add I,bc","@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"add I,de","@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"ld I,W"  ,"ld (A),I","inc I"    ,"inc Ih"  ,"dec Ih"  ,"ld Ih,B","@"      ,
	"@"      ,"add I,I" ,"ld I,(A)","dec I"    ,"inc Il"  ,"dec Il"  ,"ld Il,B","@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"inc X"   ,"dec X"   ,"ld X,B" ,"@"      ,
	"@"      ,"add I,sp","@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"ld b,Ih" ,"ld b,Il" ,"ld b,X" ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"ld c,Ih" ,"ld c,Il" ,"ld c,X" ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"ld d,Ih" ,"ld d,Il" ,"ld d,X" ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"ld e,Ih" ,"ld e,Il" ,"ld e,X" ,"@"      ,
	"ld Ih,b","ld Ih,c" ,"ld Ih,d" ,"ld Ih,e"  ,"ld Ih,h" ,"ld Ih,l" ,"ld h,X" ,"ld Ih,a",
	"ld Il,b","ld Il,c" ,"ld Il,d" ,"ld Il,e"  ,"ld Il,h" ,"ld Il,l" ,"ld l,X" ,"ld Il,a",
	"ld X,b" ,"ld X,c"  ,"ld X,d"  ,"ld X,e"   ,"ld X,h"  ,"ld X,l"  ,"@"      ,"ld X,a" ,
	"@"      ,"@"       ,"@"       ,"@"        ,"ld a,Ih" ,"ld a,Il" ,"ld a,X" ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"add a,Ih","add a,Il","add a,X","@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"adc a,Ih","adc a,Il","adc a,X","@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"sub Ih"  ,"sub Il"  ,"sub X"  ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"sbc a,Ih","sbc a,Il","sbc a,X","@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"and Ih"  ,"and Il"  ,"and X"  ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"xor Ih"  ,"xor Il"  ,"xor X"  ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"or Ih"   ,"or Il"   ,"or X"   ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"cp Ih"   ,"cp Il"   ,"cp X"   ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"fd cb"    ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"pop I"   ,"@"       ,"ex (sp),I","@"       ,"push I"  ,"@"      ,"@"      ,
	"@"      ,"jp (I)"  ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"@"       ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"      ,
	"@"      ,"ld sp,I" ,"@"       ,"@"        ,"@"       ,"@"       ,"@"      ,"@"
};

const std::array<const char*, 256> mnemonic_main =
{
	"nop"      ,"ld bc,W"  ,"ld (bc),a","inc bc"    ,"inc b"    ,"dec b"    ,"ld b,B"    ,"rlca"     ,
	"ex af,af'","add hl,bc","ld a,(bc)","dec bc"    ,"inc c"    ,"dec c"    ,"ld c,B"    ,"rrca"     ,
	"djnz R"   ,"ld de,W"  ,"ld (de),a","inc de"    ,"inc d"    ,"dec d"    ,"ld d,B"    ,"rla"      ,
	"jr R"     ,"add hl,de","ld a,(de)","dec de"    ,"inc e"    ,"dec e"    ,"ld e,B"    ,"rra"      ,
	"jr nz,R"  ,"ld hl,W"  ,"ld (A),hl","inc hl"    ,"inc h"    ,"dec h"    ,"ld h,B"    ,"daa"      ,
	"jr z,R"   ,"add hl,hl","ld hl,(A)","dec hl"    ,"inc l"    ,"dec l"    ,"ld l,B"    ,"cpl"      ,
	"jr nc,R"  ,"ld sp,W"  ,"ld (A),a" ,"inc sp"    ,"inc (hl)" ,"dec (hl)" ,"ld (hl),B" ,"scf"      ,
	"jr c,R"   ,"add hl,sp","ld a,(A)" ,"dec sp"    ,"inc a"    ,"dec a"    ,"ld a,B"    ,"ccf"      ,
	"ld b,b"   ,"ld b,c"   ,"ld b,d"   ,"ld b,e"    ,"ld b,h"   ,"ld b,l"   ,"ld b,(hl)" ,"ld b,a"   ,
	"ld c,b"   ,"ld c,c"   ,"ld c,d"   ,"ld c,e"    ,"ld c,h"   ,"ld c,l"   ,"ld c,(hl)" ,"ld c,a"   ,
	"ld d,b"   ,"ld d,c"   ,"ld d,d"   ,"ld d,e"    ,"ld d,h"   ,"ld d,l"   ,"ld d,(hl)" ,"ld d,a"   ,
	"ld e,b"   ,"ld e,c"   ,"ld e,d"   ,"ld e,e"    ,"ld e,h"   ,"ld e,l"   ,"ld e,(hl)" ,"ld e,a"   ,
	"ld h,b"   ,"ld h,c"   ,"ld h,d"   ,"ld h,e"    ,"ld h,h"   ,"ld h,l"   ,"ld h,(hl)" ,"ld h,a"   ,
	"ld l,b"   ,"ld l,c"   ,"ld l,d"   ,"ld l,e"    ,"ld l,h"   ,"ld l,l"   ,"ld l,(hl)" ,"ld l,a"   ,
	"ld (hl),b","ld (hl),c","ld (hl),d","ld (hl),e" ,"ld (hl),h","ld (hl),l","halt"      ,"ld (hl),a",
	"ld a,b"   ,"ld a,c"   ,"ld a,d"   ,"ld a,e"    ,"ld a,h"   ,"ld a,l"   ,"ld a,(hl)" ,"ld a,a"   ,
	"add a,b"  ,"add a,c"  ,"add a,d"  ,"add a,e"   ,"add a,h"  ,"add a,l"  ,"add a,(hl)","add a,a"  ,
	"adc a,b"  ,"adc a,c"  ,"adc a,d"  ,"adc a,e"   ,"adc a,h"  ,"adc a,l"  ,"adc a,(hl)","adc a,a"  ,
	"sub b"    ,"sub c"    ,"sub d"    ,"sub e"     ,"sub h"    ,"sub l"    ,"sub (hl)"  ,"sub a"    ,
	"sbc a,b"  ,"sbc a,c"  ,"sbc a,d"  ,"sbc a,e"   ,"sbc a,h"  ,"sbc a,l"  ,"sbc a,(hl)","sbc a,a"  ,
	"and b"    ,"and c"    ,"and d"    ,"and e"     ,"and h"    ,"and l"    ,"and (hl)"  ,"and a"    ,
	"xor b"    ,"xor c"    ,"xor d"    ,"xor e"     ,"xor h"    ,"xor l"    ,"xor (hl)"  ,"xor a"    ,
	"or b"     ,"or c"     ,"or d"     ,"or e"      ,"or h"     ,"or l"     ,"or (hl)"   ,"or a"     ,
	"cp b"     ,"cp c"     ,"cp d"     ,"cp e"      ,"cp h"     ,"cp l"     ,"cp (hl)"   ,"cp a"     ,
	"ret nz"   ,"pop bc"   ,"jp nz,A"  ,"jp A"      ,"call nz,A","push bc"  ,"add a,B"   ,"rst 00h"  ,
	"ret z"    ,"ret"      ,"jp z,A"   ,"cb"        ,"call z,A" ,"call A"   ,"adc a,B"   ,"rst 08h"  ,
	"ret nc"   ,"pop de"   ,"jp nc,A"  ,"out (B),a" ,"call nc,A","push de"  ,"sub B"     ,"rst 10h"  ,
	"ret c"    ,"exx"      ,"jp c,A"   ,"in a,(B)"  ,"call c,A" ,"dd"       ,"sbc a,B"   ,"rst 18h"  ,
	"ret po"   ,"pop hl"   ,"jp po,A"  ,"ex (sp),hl","call po,A","push hl"  ,"and B"     ,"rst 20h"  ,
	"ret pe"   ,"jp (hl)"  ,"jp pe,A"  ,"ex de,hl"  ,"call pe,A","ed"       ,"xor B"     ,"rst 28h"  ,
	"ret p"    ,"pop af"   ,"jp p,A"   ,"di"        ,"call p,A" ,"push af"  ,"or B"      ,"rst 30h"  ,
	"ret m"    ,"ld sp,hl" ,"jp m,A"   ,"ei"        ,"call m,A" ,"fd"       ,"cp B"      ,"rst 38h"
};

static constexpr uint8_t D = 8; // DD or FD
static constexpr uint8_t E = 4; // ED
const std::array<uint8_t, std::size_t(3) * 256> instr_len_tab = {
	// main
	1, 3, 1, 1, 1, 1, 2, 1,  1, 1, 1, 1, 1, 1, 2, 1, // 00
	2, 3, 1, 1, 1, 1, 2, 1,  2, 1, 1, 1, 1, 1, 2, 1, // 10
	2, 3, 3, 1, 1, 1, 2, 1,  2, 1, 3, 1, 1, 1, 2, 1, // 20
	2, 3, 3, 1, 1, 1, 2, 1,  2, 1, 3, 1, 1, 1, 2, 1, // 30
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 40
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 50
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 60
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 70
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 80
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // 90
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // A0
	1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, // B0
	1, 1, 3, 3, 3, 1, 2, 1,  1, 1, 3, 2, 3, 3, 2, 1, // C0
	1, 1, 3, 2, 3, 1, 2, 1,  1, 1, 3, 2, 3, D, 2, 1, // D0
	1, 1, 3, 1, 3, 1, 2, 1,  1, 1, 3, 1, 3, E, 2, 1, // E0
	1, 1, 3, 1, 3, 1, 2, 1,  1, 1, 3, 1, 3, D, 2, 1, // F0

	// ED
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 00
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 10
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 20
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 30
	2, 2, 2, 4, 2, 2, 2, 2,  2, 2, 2, 4, 2, 2, 2, 2, // 40
	2, 2, 2, 4, 2, 2, 2, 2,  2, 2, 2, 4, 2, 2, 2, 2, // 50
	2, 2, 2, 4, 2, 2, 2, 2,  2, 2, 2, 4, 2, 2, 2, 2, // 60
	2, 2, 2, 4, 2, 2, 2, 2,  2, 2, 2, 4, 2, 2, 2, 2, // 70
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 80
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 90
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // A0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // B0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // C0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // D0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // E0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // F0

	// DD or FD
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 00
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 10
	2, 4, 4, 2, 2, 2, 3, 2,  2, 2 ,4, 2, 2, 2, 3, 2, // 20
	2, 2, 2, 2, 3, 3, 4, 2,  2, 2, 2, 2, 2, 2, 2, 2, // 30
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // 40
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // 50
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // 60
	3, 3, 3, 3, 3, 3, 2, 3,  2, 2, 2, 2, 2, 2, 3, 2, // 70
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // 80
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // 90
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // A0
	2, 2, 2, 2, 2, 2, 3, 2,  2, 2, 2, 2, 2, 2, 3, 2, // B0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 4, 2, 2, 2, 2, // C0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // D0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // E0
	2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2, // F0
};

} // namespace openmsx
