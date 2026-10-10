#include "makoto-reference/ReferenceYM2608.hh"
// A fresh core must reconstruct derived sampling configuration after load.
#include "ymfm_opn.h"
#include <iostream>
#include <vector>
struct Interface : ymfm::ymfm_interface {};
int main() {
 unsigned mismatches=0;
 for(auto fidelity : {ymfm::OPN_FIDELITY_MIN,ymfm::OPN_FIDELITY_MED,ymfm::OPN_FIDELITY_MAX}) {
 for(unsigned prescale : {6U,3U,2U}) {
  Interface a,b;ymfm::ym2608_reference source(a),restored(b);
  source.set_fidelity(fidelity);restored.set_fidelity(fidelity);
  source.reset();restored.reset();
  auto write=[&](uint8_t r,uint8_t v){source.write_address(r);source.write_data(v);};
  write(0,37);write(1,0);write(7,0x3e);write(8,15);
  if(prescale!=6) source.write_address(0x2e);
  if(prescale==2) source.write_address(0x2f);
  ymfm::ym2608_reference::output_data x,y;
  for(unsigned i=0;i<137;++i) source.generate(&x);
  std::vector<uint8_t> bytes;
  ymfm::ymfm_saved_state save(bytes,true);source.save_restore(save);
  ymfm::ymfm_saved_state load(bytes,false);restored.save_restore(load);
  unsigned count=0;
  for(unsigned i=0;i<4096;++i) {
   source.generate(&x);restored.generate(&y);
   for(unsigned c=0;c<3;++c) count+=x.data[c]!=y.data[c];
  }
  mismatches+=count;
  std::cout<<"fidelity="<<unsigned(fidelity)<<" prescaler="<<prescale<<" mismatches="<<count<<'\n';
 }}
 return mismatches?1:0;
}
