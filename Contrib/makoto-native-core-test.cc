// Differential test at native chip clock edges, before host resampling.
#include "MakotoNativeChip.hh"
#include "3rdparty/ym2608/fmopn_2608rom.h"
#include <iostream>
#include <cstdlib>
struct Interface : ymfm::ymfm_interface {
 uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
  return type == ymfm::ACCESS_ADPCM_A ? YM2608_ADPCM_ROM[address & 8191]
       : uint8_t((address * 73 + (address >> 4) * 19) ^ 0x5a);
 }
};
int main() {
 unsigned samples=0, negative=0;
 for(unsigned prescale : {6U, 3U, 2U}) {
 Interface a,b; ymfm::ym2608 original(a); openmsx::MakotoNativeChip native(b);
 original.set_fidelity(ymfm::OPN_FIDELITY_MAX);
 original.reset();native.reset();
 std::array<int32_t,32> channels{};native.set_channel_output(channels.data());
 auto write = [&](unsigned reg,uint8_t value) {
  for(auto* chip : {&original,static_cast<ymfm::ym2608*>(&native)}) {
   unsigned port=reg>=256?2:0;chip->write(port,uint8_t(reg));chip->write(port+1,value);
  }
 };
 write(0x29,0x80);
 for (unsigned c=0;c<6;++c) {
  unsigned reg=(c/3)*256+c%3;
  for (unsigned slot : {0U,4U,8U,12U}) {
   write(reg+slot+0x30,1); write(reg+slot+0x40,20);
   write(reg+slot+0x50,31); write(reg+slot+0x60,0);
   write(reg+slot+0x70,0); write(reg+slot+0x80,15);
  }
  write(reg+0xb0,0x3f); write(reg+0xb4,0xc0);
  write(reg+0xa4,0x22); write(reg+0xa0,uint8_t(0x60+c*10));
  write(0x28,uint8_t(0xf0+(c/3)*4+c%3));
 }
 for (unsigned c=0;c<3;++c) {write(c*2,uint8_t(37+c*24));write(c*2+1,1);write(8+c,15);}
 write(7,0x38);
 write(0x11,63); for (unsigned c=0;c<6;++c) write(0x18+c,0xdf);
 write(0x10,0x3f);
 write(0x101,0xc0);write(0x102,0);write(0x103,0);
 write(0x104,0xff);write(0x105,0xff);write(0x109,0xff);write(0x10a,0xff);
 write(0x10b,0xff);write(0x100,0xa0);

 if(prescale!=6) { original.write_address(0x2e);native.write_address(0x2e); }
 if(prescale==2) { original.write_address(0x2f);native.write_address(0x2f); }
 const unsigned fmDiv=prescale*3, ssgDiv=prescale==6?4:prescale==3?2:1;
 std::array<int32_t,2> fm{};std::array<int32_t,3> ssg{};
 std::array<bool,16> heard{};
 for(unsigned i=0;i<360000;++i) {
  if(i==100000) { write(7,0);write(6,5);write(8,16);write(11,37);write(12,0);write(13,10); }
  if(i==200000) { write(0x10,0x3f);write(0xa4,0x26);write(0xa0,0x40); }
  ymfm::ym2608::output_data expected;original.generate(&expected);
  if(i%fmDiv==0) fm=native.clockFM();
  if(i%ssgDiv==0) ssg=native.clockSSG();
  if(fm[0]!=expected.data[0] || fm[1]!=expected.data[1] ||
     (ssg[0]+ssg[1]+ssg[2])*2/3!=expected.data[2]) {
   std::cerr<<"Mismatch prescale "<<prescale<<" sample "<<i<<'\n';return 1;
  }
  for(unsigned c=0;c<16;++c) heard[c]=heard[c] || (c>=6&&c<9 ? ssg[c-6]!=0 : channels[c*2]!=0 || channels[c*2+1]!=0);
  ++samples;
 }
 for(bool v:heard) if(!v) {std::cerr<<"Unexercised voice\n";return 2;}
 // Negative control: key off all native FM voices; comparison must detect it.
 for(unsigned c=0;c<6;++c) {native.write(0,0x28);native.write(1,uint8_t((c/3)*4+c%3));}
 for(unsigned i=0;i<36000;++i) {
  ymfm::ym2608::output_data expected; original.generate(&expected);
  if(i%fmDiv==0) fm=native.clockFM();
  if(fm[0]!=expected.data[0] || fm[1]!=expected.data[1]) ++negative;
 }
 std::cout<<"Prescaler "<<prescale<<": all 16 voices, real rhythm ROM, noise/envelope, RAM ADPCM and live writes match.\n";
 }
 if(!negative) return 3;
 std::cout<<samples<<" source samples matched; negative control detected "<<negative<<" differences.\n";
}
