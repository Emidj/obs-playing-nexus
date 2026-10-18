-- Isolated artwork integration test. No habitual scenes or external DJ process.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_update(obs_source_t*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local native=ffi.load('obs.dll');local probe,source,settings
local stage,ticks,ready,checks=0,0,0,0
local pending,lastCaptures
local function check(ok,message)checks=checks+1;assert(ok,message)end
local function update()native.obs_source_update(source,settings);ready=ticks+3 end
local function capture(label,nextStep)
 pending={count=(lastCaptures or 0)+1,nextStep=nextStep};stage=-1
 probe.playing_source_test_capture_png(source,'artwork-capture-'..label..'.png');checks=checks+1
end
local function pulse()
 ticks=ticks+1;assert(ticks<100,'Artwork timeout stage '..stage)
 local s={}
 if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then for k,v in ffi.string(b,n):gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 lastCaptures=s.captures or 0
 if pending and lastCaptures>=pending.count then local nextStep=pending.nextStep;pending=nil;nextStep() end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:00 : Artist - Artwork test\r\n');f:close()
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_data_set_int(settings,'artwork_x',500);native.obs_data_set_int(settings,'artwork_y',10);native.obs_data_set_int(settings,'artwork_width',100);native.obs_data_set_int(settings,'artwork_height',100)
  source=native.obs_source_create_private('obs_playing_next_development','Artwork isolated',settings);check(source~=nil,'Source');native.obs_source_inc_active(source);stage=1
 elseif ticks>=ready then
  if stage==1 and s.frames==1 and s.artworks==0 then capture('base',function()native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_bool(settings,'artwork_fallback',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');update();stage=2 end)
  elseif stage==2 and s.artworks==1 then check(s.sessions==1 and s.consumers==1,'Shared provider');capture('red',function()native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-blue.svg');update();stage=3 end)
  elseif stage==3 and s.artworks==1 then capture('blue',function()native.obs_data_set_bool(settings,'artwork_fallback',false);update();stage=4 end)
  elseif stage==4 and s.artworks==0 and s.frames==1 then capture('disabled',function()native.obs_data_set_bool(settings,'artwork_fallback',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'normal_preset',3);update();stage=5 end)
  elseif stage==5 and s.artworks==1 then capture('normal',function()native.obs_data_set_int(settings,'normal_preset',0);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-broken.png');update();stage=6 end)
  elseif stage==6 and s.artworks==0 and s.frames==1 then capture('broken',function()native.obs_source_dec_active(source);ready=ticks+3;stage=7 end)
  elseif stage==7 and s.frames==0 and s.consumers==0 then check(s.sessions==0,'Hidden session retained');native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');update();native.obs_source_inc_active(source);stage=8
  elseif stage==8 and s.artworks==1 then capture('resumed',function()native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;ready=ticks+3;stage=9 end)
  elseif stage==9 and s.frames==0 and s.consumers==0 then check(s.sessions==0,'Released session retained');local f=assert(io.open(script_path()..'artwork-result.txt','w'));f:write('PASS checks='..checks..'; PNG/SVG=1; fallback_off=1; corrupt=1; NORMAL=1; hide_resume_release=1');f:close();obslua.timer_remove(pulse);stage=10 end
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obslua.timer_add(pulse,200)end
function script_unload()obslua.timer_remove(pulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
