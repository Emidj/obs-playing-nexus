-- Native Cantilever redesign: real GPU, shared reader, Unicode, scroll and hide.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll')
local probe,sources,stage,ticks,checks,captures,index,waitTick=nil,{},0,0,0,0,1,0
local uploads,hash
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function history(title,artist)local f=assert(io.open(script_path()..'redesign-history.txt','wb'));f:write('21:05 : '..(artist or'Alaia & Gallo')..' - '..title..'\r\n');f:close()end
local function read()
 local buffer=ffi.new('char[4096]');local n=probe.playing_source_test_read(buffer,4096);local text=n>0 and ffi.string(buffer,n)or'';local s={}
 for k,v in text:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end
 s.hash=text:match('"captureHash":"(.-)"');s.title=text:match('"title":"(.-)"');s.animation=text:match('"animationTimer":true')~=nil;s.timer=text:match('"timer":true')~=nil;return s
end
local function capture(name)probe.playing_source_test_capture_png(sources[index],name);captures=captures+1 end
local function pulse()
 ticks=ticks+1;check(ticks<110,'Redesign timeout at '..stage)
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init')
  probe=ffi.load(script_path()..'obs-playing-next.dll');history('Lipstick')
  for peer=1,2 do
   local settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'redesign-history.txt');native.obs_data_set_int(settings,'normal_preset',3);native.obs_data_set_int(settings,'canvas_width',720);native.obs_data_set_int(settings,'normal_transition',1)
   local source=native.obs_source_create_private('obs_playing_next_development','Cantilever '..peer,settings);native.obs_data_release(settings);check(source~=nil,'Native redesign source');native.obs_source_inc_active(source);sources[#sources+1]=source
  end
  stage=1
 else
  local s=read()
  if stage==1 and s.frames==2 then
   check(s.sessions==1 and s.consumers==2 and s.animation,'Shared session and demanded ornament animation');capture('native-cantilever.png');stage=2
  elseif stage==2 and s.captures==captures then
   check(s.capturePixels>0,'Actual GPU output');uploads=s.uploads;index=2;capture('native-cantilever-peer.png');stage=3
  elseif stage==3 and s.captures==captures then
   check(s.uploads==uploads,'Second consumer reuses GPU glyphs');index=1;hash=s.hash;waitTick=ticks;stage=4
  elseif stage==4 and ticks-waitTick>=8 then capture('native-cantilever-motion.png');stage=5
  elseif stage==5 and s.captures==captures then
   check(s.hash~=hash and s.uploads==uploads,'Ornament movement without reupload');history('Jóga 日本語','Björk');waitTick=ticks;stage=6
  elseif stage==6 and ticks-waitTick>=8 and s.title=='Jóga 日本語' then capture('native-cantilever-unicode.png');stage=7
  elseif stage==7 and s.captures==captures then
   check(s.capturePixels>0,'Unicode after OUT IN');history(string.rep('Long scrolling title ',30));waitTick=ticks;stage=8
  elseif stage==8 and ticks-waitTick>=8 then capture('native-cantilever-scroll-a.png');stage=9
  elseif stage==9 and s.captures==captures then hash=s.hash;uploads=s.uploads;waitTick=ticks;stage=10
  elseif stage==10 and ticks-waitTick>=14 then capture('native-cantilever-scroll-b.png');stage=11
  elseif stage==11 and s.captures==captures then
   check(s.hash~=hash and s.uploads==uploads,'Scroll without texture upload');for _,source in ipairs(sources)do native.obs_source_dec_active(source)end;waitTick=ticks;stage=12
  elseif stage==12 and s.consumers==0 and ticks-waitTick>=3 then
   check(s.frames==0 and s.sessions==0 and not s.animation and not s.timer and s.uploads==0,'Hidden redesign releases all demand');for _,source in ipairs(sources)do native.obs_source_release(source)end;sources={}
   local f=assert(io.open(script_path()..'normal-redesign-result.txt','wb'));f:write('PASS '..checks..' native Cantilever redesign checks; captures=6; shared_reader=1; shared_gpu=1; unicode=1; scroll=1; inactive_animation=0');f:close();obs.timer_remove(pulse);stage=13
  end
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,200)end
function script_unload()obs.timer_remove(pulse);for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={}end
