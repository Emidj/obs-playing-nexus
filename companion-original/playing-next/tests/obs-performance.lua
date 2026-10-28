local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);void obs_source_update(obs_source_t*,obs_data_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);]]
local native=ffi.load('obs.dll');local probe;local sources={};local active={};local ticks,stage,ready=0,0,0;local renders;local checks=0
local function check(v,msg)checks=checks+1;assert(v,msg)end
local function profile(v)local d=native.obs_data_create_from_json('{"performance_profile":'..v..'}');native.obs_source_update(sources[1],d);native.obs_data_release(d)end
local pulse
pulse=function()
 ticks=ticks+1;assert(ticks<210,'Performance timeout stage '..stage);if ticks<ready then return end
 local s={};local raw='';if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 local function cadence(ms)check(s.animationMs==ms,'Animation cadence stage '..stage..' raw '..raw);check(raw:find('"timer":false',1,true)~=nil,'Demo must not poll');check(s.frames==1,'One active frame');check(raw:find('Performance Song',1,true)~=nil,'Track retained')end
 if stage==0 then
  local m=ffi.new('obs_module_t*[1]');check(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(m[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  for i,p in ipairs({0,2})do local d=native.obs_data_create_from_json('{"provider":"network","demo_enabled":true,"demo_title":"Performance Song","demo_artist":"Artist","pro_preset":4,"performance_profile":'..p..'}');sources[i]=native.obs_source_create_private('obs_playing_next_development','Performance '..i,d);native.obs_data_release(d);check(sources[i]~=nil,'Create')end
  native.obs_source_inc_active(sources[1]);active[1]=true;stage=1;ready=ticks+25
 elseif stage==1 and s.frames==1 then cadence(33);native.obs_source_inc_active(sources[2]);active[2]=true;stage=2;ready=ticks+15
 elseif stage==2 and s.frames==2 then check(s.animationMs==16,'Mixed demand');native.obs_source_dec_active(sources[2]);active[2]=false;stage=3;ready=ticks+15
 elseif stage==3 and s.frames==1 and s.animationMs==33 and s.consumers==1 then cadence(33);probe.playing_source_test_capture_png(sources[1],'performance-eco.png');renders=s.renders;profile(1);stage=4;ready=ticks+15
 elseif stage==4 and s.animationMs==16 then cadence(16);check(s.renders==renders,'Balanced no reraster');profile(2);stage=5;ready=ticks+15
 elseif stage==5 then cadence(16);check(s.renders==renders,'Maximum no reraster');profile(0);stage=6;ready=ticks+15
 elseif stage==6 and s.animationMs==33 then cadence(33);check(s.renders==renders,'Eco no reraster');native.obs_source_dec_active(sources[1]);active[1]=false;stage=7;ready=ticks+15
 elseif stage==7 and s.consumers==0 then check(s.frames==0 and s.consumers==0,'Hidden releases demand');check(raw:find('"animationTimer":false',1,true)~=nil,'Hidden animation stopped');native.obs_source_inc_active(sources[1]);active[1]=true;stage=8;ready=ticks+20
 elseif stage==8 and s.frames==1 then cadence(33);check(s.captures==1 and s.capturePixels>0,'Actual GPU capture');for i,source in ipairs(sources)do if active[i]then native.obs_source_dec_active(source)end;native.obs_source_release(source)end;sources={};active={};stage=9;ready=ticks+10
 elseif stage==9 and s.frames==0 and s.consumers==0 then check(s.frames==0 and s.consumers==0,'Released');local f=assert(io.open(script_path()..'performance-result.txt','w'));f:write('PASS '..checks..' checks; Eco/Balanced/Maximum; inactive and mixed demand; no demo polling; no profile reraster; retained track; hide/show; release');f:close();obs.timer_remove(pulse);return
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,100)end
function script_unload()obs.timer_remove(pulse);for i,s in ipairs(sources)do if active[i]then native.obs_source_dec_active(s)end;native.obs_source_release(s)end;sources={}end
