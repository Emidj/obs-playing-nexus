-- Developer-only native source optimization test. No product automation hooks.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);void obs_source_update(obs_source_t*,obs_data_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);]]
local native=ffi.load('obs.dll');local probe;local sources={};local active={};local ticks,stage,ready,checks=0,0,0,0;local before;local stopped;local observed="";local safePulse
local function check(v,msg)checks=checks+1;assert(v,msg)end
local function shot(i,name)probe.playing_source_test_capture_png(sources[i],'optimization-'..name..'.png')end
local function pulse()
 ticks=ticks+1;assert(ticks<210,'Optimization timeout stage '..stage);if ticks<ready then if probe then probe.playing_source_test_query()end;return end
 local s={};local raw='';if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 if stage==0 then
  local m=ffi.new('obs_module_t*[1]');check(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(m[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  for i,style in ipairs({1,4})do local d=native.obs_data_create_from_json('{"provider":"network","demo_enabled":true,"demo_title":"'..(i==1 and 'Static' or 'Vinyl')..'","demo_artist":"Artist","pro_preset":'..style..',"pro_light_intensity":80,"pro_light_pulse":'..(i==1 and 'false' or 'true')..',"normal_duration":200}')
   sources[i]=native.obs_source_create_private('obs_playing_next_development','Optimization '..i,d);native.obs_data_release(d);check(sources[i]~=nil,'Create');native.obs_source_inc_active(sources[i]);active[i]=true
  end;stage=1;ready=ticks+25
 elseif stage==1 and s.frames==2 and s.consumers==2 then shot(1,'static-a');shot(2,'vinyl-a');stage=2
 elseif stage==2 and s.captures==2 then before=s;stage=3;ready=ticks+10
 elseif stage==3 then
  local passes=s.animationPasses-before.animationPasses;check(passes>10,'Animation ticks missing '..tostring(passes)..' before '..tostring(before.animationPasses)..' raw '..raw);check(s.animationVisits-before.animationVisits==passes,'Static processed by animation');check(s.staticSkips-before.staticSkips==passes,'Static skip not recorded');check(s.dynamicCopies-before.dynamicCopies<=passes,'More than one dynamic copy per tick');check(s.renders==before.renders and s.uploads==before.uploads,'Animation reraster/upload');check(raw:find('"timer":false',1,true)~=nil,'Demo polling')
  observed='; passes='..passes..'; visits='..(s.animationVisits-before.animationVisits)..'; skips='..(s.staticSkips-before.staticSkips)..'; copies='..(s.dynamicCopies-before.dynamicCopies);shot(1,'static-b');shot(2,'vinyl-b');stage=4
 elseif stage==4 and s.captures==4 then local d=native.obs_data_create_from_json('{"demo_title":"Changed"}');native.obs_source_update(sources[1],d);native.obs_data_release(d);stage=5;ready=ticks+12
 elseif stage==5 and s.frames==2 then shot(1,'changed');stage=6
 elseif stage==6 and s.captures==5 then native.obs_source_dec_active(sources[2]);active[2]=false;stage=7;ready=ticks+5
 elseif stage==7 and s.frames==1 and s.consumers==1 and raw:find('"animationTimer":false',1,true) then stopped=s;stage=8;ready=ticks+8
 elseif stage==8 then check(s.animationPasses==stopped.animationPasses and s.dynamicCopies==stopped.dynamicCopies,'Static only timer/copies');check(s.frames==1,'Static frame missing');shot(1,'static-final');stage=9
 elseif stage==9 and s.captures==6 then check(s.capturePixels>0,'GPU capture empty');for i,source in ipairs(sources)do if active[i]then native.obs_source_dec_active(source)end;native.obs_source_release(source)end;sources={};active={};stage=10;ready=ticks+5
 elseif stage==10 and s.frames==0 and s.consumers==0 then check(raw:find('"animationTimer":false',1,true)~=nil and raw:find('"timer":false',1,true)~=nil,'Released timers');local f=assert(io.open(script_path()..'optimization-result.txt','w'));f:write('PASS '..checks..' checks; static skipped; dynamic coalesced; no animation raster/upload; static-only idle; six GPU captures; metadata change; release'..observed);f:close();obs.timer_remove(safePulse);return
 end
 if probe then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obs.timer_remove(safePulse);error(err)end end
function script_load()obs.timer_add(safePulse,100)end
function script_unload()obs.timer_remove(safePulse);for i,s in ipairs(sources)do if active[i]then native.obs_source_dec_active(s)end;native.obs_source_release(s)end;sources={}end
