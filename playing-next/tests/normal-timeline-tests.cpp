// SPDX-License-Identifier: MIT
#include "playing/visual/normal-timeline.hpp"
#include <iostream>
using namespace playing::visual::normal;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
TrackPtr track(const char *title){return std::make_shared<const playing::core::TrackState>(playing::core::TrackState{title,"Artist"});}
}
int main(){
 try{
  Timeline timeline;auto first=track("First");timeline.receive(first,0);
  check(!timeline.needsFrames(true,true),"Hidden has no demand");
  timeline.visible(true,0);auto frame=timeline.sample(0);
  check(frame.track==first&&!frame.transitioning,"First publication has no OUT");
  check(!timeline.needsFrames(false,false),"Static fitting track has no frame demand");
  check(timeline.needsFrames(true,false)&&timeline.needsFrames(false,true),"Decoration/overflow demand");
  timeline.receive(track("Next"),100);check(timeline.sample(499).track==first,"OUT retains original");
  auto latest=track(u8"Jóga 🎵 日本語");timeline.receive(latest,499);frame=timeline.sample(500);
  check(frame.track==latest&&frame.transition.alpha==0&&frame.transition.incoming,"Latest swaps at invisible midpoint");
  timeline.receive(track("After swap"),600);frame=timeline.sample(900);
  check(frame.track==latest&&frame.transitioning&&frame.transition.alpha==1,"IN finishes before pending OUT");
  frame=timeline.sample(1300);check(frame.track->title=="After swap"&&frame.transition.alpha==0,"Pending track swaps next midpoint");
  check(!timeline.sample(1700).transitioning,"Second transition finishes");
  frame=timeline.sample(2700);check(frame.scrollSeconds==1,"Scroll resets at transition end");
  const auto retained=frame.track;const auto decorations=frame.decorationMilliseconds;
  timeline.visible(false,2700);timeline.receive(track("While hidden"),9000);
  check(!timeline.needsFrames(true,true),"Hidden pending changes do not schedule");
  frame=timeline.sample(12000);check(!frame.visible&&!frame.track&&frame.decorationMilliseconds==decorations,"Hidden decoration clock frozen");
  timeline.visible(true,15000);frame=timeline.sample(15000);
  check(frame.track->title=="While hidden"&&!frame.transitioning&&frame.scrollSeconds==0,"Resume latest directly and reset scroll");
  check(frame.decorationMilliseconds==decorations,"Resume decoration at paused phase");
  check(retained->title=="After swap","Previous immutable sample retained");
  auto artwork=std::make_shared<playing::core::TrackState>(*frame.track);artwork->artwork.status=playing::core::ArtworkStatus::Missing;
  timeline.receive(artwork,15010);frame=timeline.sample(15010);
  check(!frame.transitioning&&frame.track==artwork,"Artwork-only update does not transition text");
  for(int i=0;i<64;++i)timeline.receive(track(std::to_string(i).c_str()),15100+i);
  frame=timeline.sample(15500);check(frame.track->title=="63","Burst coalesces last pending track");
  frame=timeline.sample(15900);check(!frame.transitioning,"Burst does not create 64 queued transitions");
  timeline.receive({},16000);frame=timeline.sample(16000);
  check(!frame.visible&&!timeline.needsFrames(true,true),"Unavailable clears presentation/demand");
  bool rejected=false;try{timeline.sample(15999);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Clock regression rejected");
  bool wrongThread=false;std::thread other([&]{try{timeline.needsFrames(true,true);}catch(const std::logic_error&){wrongThread=true;}});other.join();check(wrongThread,"GUI owner contract");
  timeline.configure(800,Transition::Slide,16000);timeline.receive(track("Slide"),16010);timeline.receive(track("Slide next"),16020);
  frame=timeline.sample(16220);check(frame.transition.offsetYPercent==-32.5,"Slide state machine OUT");
  frame=timeline.sample(16420);check(frame.transition.offsetYPercent==65,"Slide state machine IN");
  timeline.configure(800,Transition::Slide,16420);check(timeline.sample(16420).transitioning,"Identical config preserves transition");
  timeline.configure(800,Transition::Zoom,16420);frame=timeline.sample(16420);
  check(!frame.transitioning&&frame.scrollSeconds==0&&frame.decorationMilliseconds==0,"Changed style resets original visual clock without changing track");
  auto identified=std::make_shared<playing::core::TrackState>(playing::core::TrackState{"Same title","Artist"});
  identified->identity=playing::core::TrackIdentity{"library","one"};
  timeline.visible(false,17000);timeline.receive(identified,17000);timeline.visible(true,17000);
  auto otherIdentity=std::make_shared<playing::core::TrackState>(*identified);otherIdentity->identity->value="two";
  timeline.receive(otherIdentity,17010);frame=timeline.sample(17010);
  check(frame.transitioning&&frame.track==identified,"Authoritative identity-only change did not transition");
  check(timeline.sample(17410).track==otherIdentity,"Identity-only midpoint did not swap");
  timeline.sample(17810);
  auto coverOnly=std::make_shared<playing::core::TrackState>(*otherIdentity);coverOnly->artwork.status=playing::core::ArtworkStatus::Missing;
  timeline.receive(coverOnly,17820);check(!timeline.sample(17820).transitioning,"Cover enrichment retriggered identity transition");
  auto unknown=std::make_shared<playing::core::TrackState>(*coverOnly);unknown->identity.reset();
  timeline.receive(unknown,17830);check(!timeline.sample(17830).transitioning,"Unknown identity fabricated a track change");
  std::cout<<"PASS "<<checks<<" NORMAL timeline checks; GPU parity validated separately\n";return 0;
 }catch(const std::exception&error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
