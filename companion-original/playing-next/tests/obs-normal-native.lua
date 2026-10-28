-- Native Prism/Air GPU, cache, movement and inactive lifecycle in owned OBS only.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);uint32_t obs_source_get_height(obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll')
local probe,sources={},{}
local stage,ticks,checks,index,waitTick=0,0,0,1,0
local captures,hash,uploads=0,nil,0
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function history(title)local f=assert(io.open(script_path()..'normal-native-history.txt','wb'));f:write('21:05 : Alaia & Gallo - '..title..'\r\n');f:close()end
local function read()
 local buffer=ffi.new('char[4096]');local n=probe.playing_source_test_read(buffer,4096);local text=n>0 and ffi.string(buffer,n)or'';local s={}
 for k,v in text:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end
 s.hash=text:match('"captureHash":"(.-)"');s.title=text:match('"title":"(.-)"');s.animation=text:match('"animationTimer":true')~=nil;s.timer=text:match('"timer":true')~=nil;return s
end
local function capture(name)probe.playing_source_test_capture_png(ffi.cast('obs_source_t*',sources[index]),name);captures=captures+1 end
local function pulse()
 ticks=ticks+1;check(ticks<110,'NORMAL native timeout at '..stage)
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Native module open');check(native.obs_init_module(module[0]),'Native init')
  probe=ffi.load(script_path()..'obs-playing-next.dll');history('Lipstick')
  for preset=1,2 do for peer=1,2 do
   local settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'normal-native-history.txt');native.obs_data_set_int(settings,'normal_preset',preset);native.obs_data_set_int(settings,'canvas_width',720)
   native.obs_data_set_int(settings,'normal_transition',preset==1 and 1 or 2)
   local source=native.obs_source_create_private('obs_playing_next_development','Normal '..preset..' peer '..peer,settings);native.obs_data_release(settings);check(source~=nil,'Native NORMAL source');native.obs_source_inc_active(source);sources[#sources+1]=source
  end end
  stage=1
 else
  local s=read()
  if stage==1 and s.frames==4 then
   check(s.sessions==1 and s.consumers==4,'NORMAL reader sharing');check(not s.animation,'Static NORMAL animation timer')
   check(native.obs_source_get_height(sources[1])==166 and native.obs_source_get_height(sources[3])==158,'Native dynamic heights');capture('native-prism.png');stage=2
  elseif stage==2 and s.captures==captures then
   check(s.capturePixels>0,'Prism GPU pixels');hash=s.hash;uploads=s.uploads;index=2;capture('native-prism-peer.png');stage=3
  elseif stage==3 and s.captures==captures then
   check(s.hash==hash and s.uploads==uploads,'Prism shared GPU cache');index=3;capture('native-air.png');stage=4
  elseif stage==4 and s.captures==captures then
   check(s.capturePixels>0 and s.hash~=hash,'Air GPU output');hash=s.hash;uploads=s.uploads;index=4;capture('native-air-peer.png');stage=5
  elseif stage==5 and s.captures==captures then
   check(s.hash==hash and s.uploads==uploads,'Air shared GPU cache');index=1;capture('native-prism-black.png');stage=51
  elseif stage==51 and s.captures==captures then capture('native-prism-white.png');stage=52
  elseif stage==52 and s.captures==captures then index=3;capture('native-air-black.png');stage=53
  elseif stage==53 and s.captures==captures then capture('native-air-white.png');stage=54
  elseif stage==54 and s.captures==captures then
   history('Changed native track');waitTick=ticks;stage=6
  elseif stage==6 and ticks-waitTick>=7 and s.title=='Changed native track' and not s.animation then
   index=3;capture('native-air-changed.png');stage=7
  elseif stage==7 and s.captures==captures then
   check(s.capturePixels>0 and s.hash~=hash,'Native OUT IN changed text');history(string.rep('Long scrolling title ',30));waitTick=ticks;stage=8
  elseif stage==8 and ticks-waitTick>=7 and s.animation then
   index=1;capture('native-prism-scroll-a.png');stage=9
  elseif stage==9 and s.captures==captures then
   hash=s.hash;uploads=s.uploads;waitTick=ticks;stage=10
  elseif stage==10 and ticks-waitTick>=14 then capture('native-prism-scroll-b.png');stage=11
  elseif stage==11 and s.captures==captures then
   check(s.hash~=hash and s.uploads==uploads,'Native scrolling without GPU reupload');for _,source in ipairs(sources)do native.obs_source_dec_active(source)end;waitTick=ticks;stage=12
  elseif stage==12 and s.consumers==0 and ticks-waitTick>=3 then
   check(s.frames==0 and s.sessions==0 and not s.animation and not s.timer and s.uploads==0,'Inactive NORMAL retained work/cache')
   for _,source in ipairs(sources)do native.obs_source_release(source)end;sources={}
   local f=assert(io.open(script_path()..'normal-native-result.txt','wb'));f:write('PASS '..checks..' native Prism/Air GPU checks; captures=11; opaque_mattes=1; shared_reader=1; shared_gpu=1; scroll=1; inactive_animation=0; original_visual_parity_pending=1');f:close();obs.timer_remove(pulse);stage=13
  end
 end
 if stage~=0 then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,200)end
function script_unload()obs.timer_remove(pulse);for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={}end
