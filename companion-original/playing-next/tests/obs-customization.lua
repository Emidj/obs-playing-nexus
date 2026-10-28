-- Developer-only real GPU customization / reset test, four consumers.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);void obs_data_unset_user_value(obs_data_t*,const char*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);void obs_source_update(obs_source_t*,obs_data_t*);obs_data_t* obs_source_get_settings(const obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll');local probe;local sources={};local ticks,stage,ready=0,0,0;local done=false
local keys={'text_title_font','text_artist_font','text_title_color','text_artist_color','title_outline_width','title_outline_color','title_shadow_offset','title_shadow_color','artist_outline_width','artist_outline_color','artist_shadow_offset','artist_shadow_color'}
local function capture(name)for i,source in ipairs(sources)do probe.playing_source_test_capture_png(source,'custom-'..i..'-'..name..'-transition-base.png')end end
local pulse
pulse=function()
 ticks=ticks+1;assert(ticks<220,'Customization stage timeout '..stage);if ticks<ready then return end
 local s={};if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then for k,v in ffi.string(b,n):gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');assert(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0);assert(native.obs_init_module(module[0]));probe=ffi.load(script_path()..'obs-playing-next.dll')
  for i,style in ipairs({'"normal_preset":1','"normal_preset":3','"pro_preset":1','"text_layout":0'})do
   local settings=native.obs_data_create_from_json('{"provider":"network","demo_enabled":true,"demo_title":"Custom Song","demo_artist":"Artist","pro_light_intensity":0,"normal_gap":12,'..style..'}')
   local source=native.obs_source_create_private('obs_playing_next_development','Customization '..i,settings);assert(source~=nil);native.obs_data_release(settings);native.obs_source_inc_active(source);sources[i]=source
  end
  stage=1;ready=ticks+25
 elseif stage==1 and s.frames==4 then capture('default');stage=2;ready=ticks+10
 elseif stage==2 and s.captures==4 then
  for _,source in ipairs(sources)do local settings=native.obs_data_create_from_json('{"text_title_font":{"face":"Arial","size":36,"flags":3},"text_artist_font":{"face":"Segoe UI","size":20,"flags":0},"text_title_color":4283498495,"text_artist_color":4291611904,"title_outline_width":2,"title_outline_color":4278190080,"title_shadow_offset":3,"title_shadow_color":2684354560,"artist_outline_width":1,"artist_shadow_offset":2}')
   native.obs_source_update(source,settings);native.obs_data_release(settings)
  end
  stage=3;ready=ticks+30
 elseif stage==3 and s.frames==4 then capture('styled');stage=4;ready=ticks+10
 elseif stage==4 and s.captures==8 then
  for _,source in ipairs(sources)do local settings=native.obs_source_get_settings(source);for _,key in ipairs(keys)do native.obs_data_unset_user_value(settings,key)end;native.obs_source_update(source,settings);native.obs_data_release(settings)end
  stage=5;ready=ticks+30
 elseif stage==5 and s.frames==4 then capture('reset');stage=6;ready=ticks+10
 elseif stage==6 and s.captures==12 then
  for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={};stage=7;ready=ticks+10
 elseif stage==7 and s.consumers==0 and s.frames==0 then
  local file=assert(io.open(script_path()..'customization-result.txt','w'));file:write('PASS four real sources; 12 GPU captures; explicit font/color/effects; reset; released consumers');file:close();done=true;obs.timer_remove(pulse)
 end
 if probe and not done then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,100)end
function script_unload()obs.timer_remove(pulse);for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={}end
