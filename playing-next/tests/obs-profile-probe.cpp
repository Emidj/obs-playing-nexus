// SPDX-License-Identifier: MIT
// Test-only output sink: native callback, no Lua/Qt on the video thread.
#include <obs.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <array>
#include <mutex>
#include <util/platform.h>
namespace {
std::atomic<unsigned long long> frames{0};
std::atomic<unsigned> nonBlack{0};
unsigned width=0,height=0;
std::mutex deliveriesMutex;
std::array<std::pair<std::uint64_t,std::uint64_t>,512> deliveries{};
std::size_t deliveryIndex=0;
bool started=false; // Calling script thread only.
void receive(void *,video_data *frame){
 unsigned ink=0;
 if(frame->data[0])for(unsigned y=0;y<8;++y)for(unsigned x=0;x<16;++x)
  if(frame->data[0][((2*y+1)*height/16)*frame->linesize[0]+(2*x+1)*width/32]>20)++ink;
 nonBlack.store(ink,std::memory_order_relaxed);frames.fetch_add(1,std::memory_order_relaxed);
 {std::lock_guard<std::mutex> lock(deliveriesMutex);deliveries[deliveryIndex++%deliveries.size()]={frame->timestamp,os_gettime_ns()};}
}
}
extern "C" __declspec(dllexport) bool playing_profile_start(){if(!started){const auto info=video_output_get_info(obs_get_video());if(!info||info->format!=VIDEO_FORMAT_NV12)return false;width=info->width;height=info->height;frames=0;obs_add_raw_video_callback(nullptr,receive,nullptr);started=true;}return true;}
extern "C" __declspec(dllexport) void playing_profile_stop(){if(started){obs_remove_raw_video_callback(receive,nullptr);started=false;}}
extern "C" __declspec(dllexport) std::uint64_t playing_profile_delivery(std::uint64_t timestamp){
 std::lock_guard<std::mutex> lock(deliveriesMutex);
 for(const auto &entry:deliveries)if(entry.first==timestamp)return entry.second;
 return 0;
}
extern "C" __declspec(dllexport) int playing_profile_read(char *buffer,int capacity){
 if(!buffer||capacity<2)return 0;
 char text[512];const int size=std::snprintf(text,sizeof(text),"{\"outputFrames\":%llu,\"nonBlackSamples\":%u,\"totalFrames\":%u,\"laggedFrames\":%u,\"activeFps\":%.6f,\"averageFrameMs\":%.6f}",frames.load(std::memory_order_relaxed),nonBlack.load(std::memory_order_relaxed),obs_get_total_frames(),obs_get_lagged_frames(),obs_get_active_fps(),double(obs_get_average_frame_time_ns())/1000000);
 if(size<0||size>=capacity||size>=int(sizeof(text)))return 0;
 std::memcpy(buffer,text,static_cast<size_t>(size));buffer[size]=0;return size;
}
