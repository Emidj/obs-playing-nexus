-- Multilayer product source in an isolated OBS portable; all input is fixture.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_update(obs_source_t*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
obs_properties_t* obs_source_properties(const obs_source_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);size_t obs_property_list_item_count(obs_property_t*);void obs_properties_destroy(obs_properties_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local native=ffi.load('obs.dll');local probe,source,settings
local stage,ticks,ready,checks=0,0,0,0;local pending,rasters,uploads
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function profile(n)native.obs_data_set_int(settings,'layer_animation',n);native.obs_source_update(source,settings);ready=ticks+3 end
local function capture(label,s,nextStep)pending={count=(s.captures or 0)+1,nextStep=nextStep};stage=-1;probe.playing_source_test_capture_png(source,'motion-capture-'..label..'.png')end
local function pulse()
 ticks=ticks+1;assert(ticks<120,'Motion timeout stage '..stage)
 local s={};if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then local raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end;s.animation=raw:find('"animationTimer":true',1,true)~=nil end end
 if pending and (s.captures or 0)>=pending.count then local nextStep=pending.nextStep;pending=nil;nextStep(s)end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(module[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:00 : Artist - Multilayer test\r\n');f:close()
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_bool(settings,'artwork_fallback',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'artwork_x',500);native.obs_data_set_int(settings,'artwork_y',10);native.obs_data_set_int(settings,'artwork_width',100);native.obs_data_set_int(settings,'artwork_height',100)
  source=native.obs_source_create_private('obs_playing_next_development','Multilayer isolated',settings);check(source~=nil,'Source');local p=native.obs_source_properties(source);check(native.obs_property_list_item_count(native.obs_properties_get(p,'layer_animation'))==3,'Motion selector');native.obs_properties_destroy(p);native.obs_source_inc_active(source);stage=1
 elseif ticks>=ready then
  if stage==1 and s.artworks==1 then check(not s.animation,'Default idle timer');capture('base',s,function()profile(1);stage=2 end)
  elseif stage==2 and s.artworks==1 and s.animation then capture('float-a',s,function(q)rasters=q.renders;uploads=q.uploads;ready=ticks+9;stage=3 end)
  elseif stage==3 then check(s.renders==rasters and s.uploads==uploads,'Animation rereads/rasterizes/uploads');check(s.sessions==1 and s.consumers==1,'Session changed');capture('float-b',s,function()profile(2);stage=4 end)
  elseif stage==4 and s.artworks==1 and s.animation then capture('breathe-a',s,function(q)rasters=q.renders;uploads=q.uploads;ready=ticks+9;stage=5 end)
  elseif stage==5 then check(s.renders==rasters and s.uploads==uploads,'Breathing raster/upload');capture('breathe-b',s,function()native.obs_source_dec_active(source);ready=ticks+3;stage=6 end)
  elseif stage==6 and s.frames==0 and s.consumers==0 then check(not s.animation and s.sessions==0,'Hidden animation demand');profile(0);native.obs_source_inc_active(source);stage=7
  elseif stage==7 and s.artworks==1 then check(not s.animation,'Static resumed timer');capture('static-again',s,function()native.obs_data_set_int(settings,'normal_preset',1);profile(2);stage=8 end)
  elseif stage==8 and s.animation and s.artworks==1 then capture('prism-text-a',s,function()ready=ticks+9;stage=9 end)
  elseif stage==9 then capture('prism-text-b',s,function()native.obs_data_set_int(settings,'normal_preset',3);profile(1);stage=10 end)
  elseif stage==10 and s.animation and s.artworks==1 then capture('cantilever-text-a',s,function()ready=ticks+9;stage=11 end)
  elseif stage==11 then capture('cantilever-text-b',s,function()native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;ready=ticks+3;stage=12 end)
  elseif stage==12 and s.frames==0 and s.consumers==0 then check(not s.animation and s.sessions==0,'Release demand');local f=assert(io.open(script_path()..'layer-animation-result.txt','w'));f:write('PASS checks='..checks..'; layers=title,artist,artwork; profiles=2; native_text_planes=2; raster_upload_stable=1; hidden_and_static_timer=0');f:close();obslua.timer_remove(pulse);stage=13 end
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obslua.timer_add(pulse,200)end
function script_unload()obslua.timer_remove(pulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
