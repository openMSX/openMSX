// Standalone regression for the small YMFM channel-output adapter.
// Build with ymfm_opn.cpp, ymfm_ssg.cpp and ymfm_adpcm.cpp, C++17 or newer.
#include "ymfm_opn.h"
#include "MakotoMix.hh"
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

struct Interface : ymfm::ymfm_interface {
 uint8_t ymfm_external_read(ymfm::access_class, uint32_t address) override {
  return uint8_t((address * 73 + (address >> 4) * 19) ^ 0x5a);
 }
};
static void require(bool value, const char* message) {
 if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
static std::vector<uint8_t> state(ymfm::ym2608& chip) {
 std::vector<uint8_t> result;
 ymfm::ymfm_saved_state s(result, true); chip.save_restore(s); return result;
}
int main() {
 Interface a, b; ymfm::ym2608 original(a), separated(b);
 std::array<int32_t, 32> channels{};
 separated.set_channel_output(channels.data());
 original.reset(); separated.reset();
 auto write = [&](unsigned reg, uint8_t value) {
  unsigned port = reg >= 256 ? 2 : 0;
  for (auto* chip : {&original, &separated}) {
   chip->write(port, uint8_t(reg)); chip->write(port + 1, value);
  }
 };
 write(0x29,0x80);
 for (unsigned c=0;c<6;++c) {
  unsigned reg=(c/3)*256+c%3;
  for (unsigned slot : {0U,4U,8U,12U}) {
   write(reg+slot+0x30,1); write(reg+slot+0x40,0);
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
 std::array<bool,16> heard{};
 unsigned samples=0;
 unsigned clipped=0; float maxError=0; openmsx::MakotoMix mix;
 for (unsigned prescale : {0x2dU,0x2eU,0x2fU}) {
  original.write_address(uint8_t(prescale));separated.write_address(uint8_t(prescale));
  for (unsigned i=0;i<120000;++i) {
   ymfm::ym2608::output_data x,y;
   original.generate(&x);separated.generate(&y);
   for(unsigned o=0;o<3;++o) require(x.data[o]==y.data[o],"Mixed output differs");
   auto& ssg=separated.ssg_output();
   for(float gain : {0.0f, 0.5f/4.3f, 1.0f/4.3f}) {
    mix.update(channels,ssg.data,y.data,gain);
    for(unsigned side=0;side<2;++side) {
     float sum=0; int raw=0;
     for(unsigned c=0;c<16;++c) {sum+=mix.voices[2*c+side]; if(c<6||c>=9)raw+=channels[2*c+side];}
     float direct=float(y.data[side])+float(y.data[2])*gain;
     maxError=std::max(maxError,std::abs(sum-direct));
     require(std::abs(sum-direct)<=0.03125f, "Voice mix differs beyond floating-point rounding");
     if(raw>32767||raw< -32768) ++clipped;
    }
   }
   require((ssg.data[0]+ssg.data[1]+ssg.data[2])*2/3==y.data[2],"Separated SSG differs");
   for(unsigned c=0;c<16;++c) {
    if(c>=6&&c<9) heard[c]=heard[c]||ssg.data[c-6]!=0;
    else heard[c]=heard[c]||channels[2*c]!=0||channels[2*c+1]!=0;
   }
   if(i%10000==0) require(state(original)==state(separated),"Chip state differs");
   ++samples;
  }
 }
 // Cancellation must not make separate voices look silent.
 std::array<int32_t,32> cancel{}; cancel[0]=1234; cancel[2]=-1234;
 int32_t zero[3]={0,0,0}; mix.update(cancel,zero,zero,0);
 require(mix.voices[0]+mix.voices[2]==0 && mix.voices[0]!=0 && mix.voices[2]!=0,"Cancellation lost channel information");
 cancel.fill(0);mix.update(cancel,zero,zero,0);
 for(float v:mix.voices)require(v==0,"Silence produced nonzero voice");
 for(unsigned c=0;c<16;++c) { if(!heard[c]) std::cerr<<"Missing channel "<<c<<'\n'; require(heard[c],"Voice not exercised"); }
 require(state(original)==state(separated),"Final state differs");
 uint32_t hash=2166136261U;
 for(auto value:state(original)) hash=(hash^value)*16777619U;
 require(clipped>0,"Clipping not exercised");
 std::cout<<"Clipped sample/gain cases: "<<clipped<<"; maximum difference from direct YMFM mix: "<<maxError<<" chip units\n";
 std::cout<<"Clipping-fixture core state FNV-1a: "<<std::hex<<hash<<std::dec<<" ("<<state(original).size()<<" bytes)\n";
 std::cout<<samples<<" samples match the original mixed path exactly; all 16 voices exercised.\n";
}
