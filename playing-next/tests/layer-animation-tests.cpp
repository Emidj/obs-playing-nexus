// SPDX-License-Identifier: MIT
#include "playing/visual/animation-profiles.hpp"
#include <QGuiApplication>
#include <iostream>
#include <limits>
using namespace playing::visual::animation;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok)throw std::runtime_error("Animation check "+std::to_string(checks));}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}check(failed);}}
int main(int argc,char **argv){QGuiApplication app(argc,argv);try{
 const Transform a{},b{10,20,.5,2,30};
 Clip once(1000,200,Loop::Once,{{0,a,Ease::Linear},{1,b,Ease::Linear}});
 check(same(once.sample(0),a)&&same(once.sample(200),a));check(once.sample(700).x==5&&once.sample(700).opacity==.75);check(same(once.sample(1200),b)&&!once.needsFrames(1200));
 Clip ping(1000,0,Loop::PingPong,{{0,a,Ease::Cosine},{1,b,Ease::Linear}});
 check(same(ping.sample(1000),b)&&same(ping.sample(2000),a));check(std::abs(ping.sample(500).x-5)<1e-10);check(same(ping.sample(1500),ping.sample(500)));
 Clip repeat(1000,0,Loop::Repeat,{{0,a,Ease::Linear},{.5,b,Ease::Linear},{1,a,Ease::Linear}});check(same(repeat.sample(1000),a)&&repeat.sample(1250).x==5);
 Clip step(1000,0,Loop::Once,{{0,a,Ease::Step},{1,b,Ease::Linear}});check(same(step.sample(999),a)&&same(step.sample(1000),b));
 Clip fixed(1000,500,Loop::Repeat,{{0,a,Ease::Linear},{1,a,Ease::Linear}});check(!fixed.needsFrames(0));
 rejects([&]{Clip invalid(100,0,Loop::Once,{{0,a},{1,b}});});rejects([&]{Clip invalid(1000,0,Loop::Repeat,{{0,a},{1,b}});});rejects([&]{Clip invalid(1000,0,Loop::Once,{{.1,a},{1,b}});});rejects([&]{Clip invalid(1000,0,Loop::Once,{{0,a},{0,b}});});
 auto bad=b;bad.opacity=2;rejects([&]{Clip invalid(1000,0,Loop::Once,{{0,a},{1,bad}});});bad=b;bad.scale=0;rejects([&]{Clip invalid(1000,0,Loop::Once,{{0,a},{1,bad}});});
 rejects([&]{once.sample(std::numeric_limits<double>::quiet_NaN());});rejects([&]{once.sample(-1);});
 Clock clock;clock.visible(true,10);check(clock.elapsed(510)==500);clock.visible(false,510);check(clock.elapsed(10000)==500);clock.visible(true,10000);check(clock.elapsed(10500)==1000);clock.reset(10500);check(clock.elapsed(10500)==0);rejects([&]{clock.elapsed(10499);});
 check(profiles().size()==3&&!program(0).needsFrames(0));auto sample=program(1).sample(4000);check(sample[1].x==6&&sample[2].x<0&&sample[3].scale>1);check(program(2).sample(3000)[3].angle>0);
 const QByteArray valid=R"({"format":"obs-playing.animations","version":1,"profiles":[{"setting":0,"name":"Default","layers":[]}]})";
 check(parseProfiles(valid).size()==1);rejects([&]{parseProfiles(valid+"!");});rejects([&]{parseProfiles(QByteArray(65537,'x'));});rejects([&]{parseProfiles(QByteArray(valid).replace("\"version\":1","\"version\":2"));});rejects([&]{parseProfiles(QByteArray(valid).replace("\"layers\":[]","\"layers\":[],\"script\":\"bad\""));});
 for(int i=0;i<1000;++i){const double t=i*17.0;const auto frames=program(1).sample(t);check(frames[1].x>=0&&frames[1].x<=6&&frames[2].x>=-4&&frames[2].x<=0&&frames[3].scale>=1&&frames[3].scale<=1.04);}
 std::cout<<"PASS "<<checks<<" layer animation checks\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
