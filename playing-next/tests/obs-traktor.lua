-- Opt-in real Traktor test; only an isolated portable OBS, never habitual scenes.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);
void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);
int64_t obs_data_get_int(obs_data_t*,const char*);bool obs_data_get_bool(obs_data_t*,const char*);const char* obs_data_get_string(obs_data_t*,const char*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
obs_properties_t* obs_source_properties(const obs_source_t*);void obs_properties_destroy(obs_properties_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);
size_t obs_property_list_item_count(obs_property_t*);const char* obs_property_list_item_string(obs_property_t*,size_t);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local native=ffi.load('obs.dll');local probe,first,second,settings
local stage,ticks,checks=0,0,0;local title='';local peerActive=false
local function check(ok,message)checks=checks+1;assert(ok,message)end
local function pulse()
 ticks=ticks+1;assert(ticks<110,'Traktor OBS timeout stage '..stage)
 local s={}
 if probe then local b=ffi.new('char[8192]');local n=probe.playing_source_test_read(b,8192);if n>0 then
  local data=native.obs_data_create_from_json(b);if data~=nil then
   for _,key in ipairs({'frames','artworks','sessions','consumers','captures','capturePixels'})do s[key]=tonumber(native.obs_data_get_int(data,key))end
   s.title=ffi.string(native.obs_data_get_string(data,'title'));s.timer=native.obs_data_get_bool(data,'timer');native.obs_data_release(data)
  end
 end end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module initialization');probe=ffi.load(script_path()..'obs-playing-next.dll')
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','traktor');native.obs_data_set_int(settings,'mode',1)
  native.obs_data_set_int(settings,'preset_family',1);native.obs_data_set_int(settings,'preset_style',1);native.obs_data_set_int(settings,'pro_preset',1);native.obs_data_set_int(settings,'normal_preset',0)
  native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',367);native.obs_data_set_bool(settings,'demo_enabled',false);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_bool(settings,'artwork_fallback',false);native.obs_data_set_bool(settings,'twitch_profile_auto',false)
  first=native.obs_source_create_private('obs_playing_next_development','Real Traktor first',settings);second=native.obs_source_create_private('obs_playing_next_development','Real Traktor peer',settings);check(first~=nil and second~=nil,'Traktor sources')
  local props=native.obs_source_properties(first);local list=native.obs_properties_get(props,'provider');local found=false
  for i=0,tonumber(native.obs_property_list_item_count(list))-1 do if ffi.string(native.obs_property_list_item_string(list,i))=='traktor' then found=true end end
  check(found,'Traktor absent from Add Source properties');native.obs_properties_destroy(props)
  native.obs_source_inc_active(first);native.obs_source_inc_active(second);peerActive=true;stage=1
 elseif stage==1 and s.frames==2 and s.artworks==2 and s.title and s.title~='' then
  check(s.sessions==1 and s.consumers==2,'Two sources did not share Traktor worker');title=s.title
  probe.playing_source_test_capture_png(first,'traktor-real.png');stage=2
 elseif stage==2 and s.captures==1 then
  check(s.capturePixels>0,'Traktor frame did not render');native.obs_source_dec_active(first);native.obs_source_release(first);first=nil;stage=3
 elseif stage==3 and s.frames==1 and s.consumers==1 then
  check(s.sessions==1 and s.artworks==1,'First source stopped shared reader');native.obs_source_dec_active(second);peerActive=false;native.obs_source_release(second);second=nil;native.obs_data_release(settings);settings=nil;stage=4
 elseif stage==4 and s.consumers==0 then
  check(s.sessions==0 and s.frames==0 and not s.timer,'Last consumer retained reader/timer')
  local f=assert(io.open(script_path()..'traktor-result.txt','w'));f:write('PASS checks='..checks..'; real_traktor=1; title='..title..'; cover=1; shared_consumers=2; release=1; render=1');f:close();obslua.timer_remove(pulse);stage=5
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obslua.timer_add(pulse,200)end
function script_unload()obslua.timer_remove(pulse);if first then native.obs_source_dec_active(first);native.obs_source_release(first)end;if second then if peerActive then native.obs_source_dec_active(second)end;native.obs_source_release(second)end;if settings then native.obs_data_release(settings)end end
