#include "Source/pb/PedalFactory.h"
#include <cstdio>
int main(){ const float fs=48000; for(int c=0;c<2;++c){ auto ch=pb::defaultChain(c); std::vector<std::unique_ptr<pb::Pedal>> ps; for(auto k:ch){ps.push_back(pb::make(k)); ps.back()->prepare(fs);} 
  std::vector<float> b(48000*2); for(size_t i=0;i<b.size();++i) b[i]=0.2f*std::sin(2*pb::kPi*196.f*i/fs); pb::P p; p.s[0]=1;
  for(size_t i=0;i<ps.size();++i){ ps[i]->process(b.data(),(int)b.size(),p); }
  float pk=0; bool bad=false; for(float v:b){ if(!std::isfinite(v))bad=true; pk=std::max(pk,std::fabs(v)); }
  printf("Kette %c: %zu Pedale alle an, Spitze %.2f %s\n",c?'R':'A',ch.size(),pk,bad?"NAN!":"stabil"); } }
