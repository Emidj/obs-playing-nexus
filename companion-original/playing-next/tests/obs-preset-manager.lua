-- Isolated development fixture: real manager controls and native source.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_bool(obs_data_t*,const char*,bool);
int64_t obs_data_get_int(obs_data_t*,const char*);const char* obs_data_get_string(obs_data_t*,const char*);bool obs_data_get_bool(obs_data_t*,const char*);bool obs_data_has_user_value(obs_data_t*,const char*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);obs_source_t* obs_source_create(const char*,const char*,obs_data_t*,obs_data_t*);void obs_source_release(obs_source_t*);obs_data_t* obs_source_get_settings(const obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);uint32_t obs_source_get_width(obs_source_t*);uint32_t obs_source_get_height(obs_source_t*);
typedef struct obs_scene obs_scene_t;typedef struct obs_scene_item obs_sceneitem_t;
obs_scene_t* obs_scene_create(const char*);obs_sceneitem_t* obs_scene_add(obs_scene_t*,obs_source_t*);void obs_sceneitem_remove(obs_sceneitem_t*);obs_source_t* obs_scene_get_source(const obs_scene_t*);void obs_scene_release(obs_scene_t*);
void obs_frontend_set_current_scene(obs_source_t*);
obs_properties_t* obs_source_properties(const obs_source_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);void obs_properties_destroy(obs_properties_t*);void* obs_obj_get_data(void*);bool obs_property_button_clicked(obs_property_t*,void*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_manager_command(int,int);
void playing_source_test_capture_png(obs_source_t*,const char*);
void playing_source_test_scene_create(obs_data_t*);void playing_source_test_scene_remove(void);obs_source_t* obs_get_source_by_name(const char*);bool obs_source_removed(const obs_source_t*);
]]
local native=ffi.load('obs.dll');local probe,source;local stage,ticks,checks,ready=0,0,0,0;local done=false;local removedSetting
ffi.cdef[[void obs_source_update(obs_source_t*,obs_data_t*);obs_data_t* obs_data_create_from_json(const char*);]]
local function flag(name)local f=io.open(script_path()..name,'r');if f then f:close();return true end;return false end
local inScene=flag('manager-scene.flag');local keepOpen=flag('manager-keep-open.flag');local keepSource=flag('manager-keep-source.flag')
local directSave=flag('manager-direct-save.flag')
local fileExchange=flag('manager-file-exchange.flag');local savedId
local function check(ok,reason)checks=checks+1;assert(ok,reason)end
local pulse
local function finish()local f=assert(io.open(script_path()..'preset-manager-result.txt','w'));f:write('PASS '..checks..' real OBS preset manager checks; scene='..tostring(inScene)..'; openOnExit='..tostring(keepOpen)..'; liveOnExit='..tostring(keepSource));f:close();done=true;obs.timer_remove(pulse)end
local function open(save)
 local props=native.obs_source_properties(source);local button=native.obs_properties_get(props,save and 'preset_save_as' or 'preset_manager');check(button~=nil,'Manager property missing');native.obs_property_button_clicked(button,native.obs_obj_get_data(source));native.obs_properties_destroy(props)
end
pulse=function()
 ticks=ticks+1;assert(ticks<210,'Manager timeout stage '..stage);if ticks<ready then return end
 local s={};if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then for k,v in ffi.string(b,n):gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  local settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','network');native.obs_data_set_string(settings,'receiver_profile',script_path()..'private-link.pcs');native.obs_data_set_string(settings,'streamer_image',script_path()..'private-photo.png');native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_string(settings,'demo_title','Manager test song');native.obs_data_set_string(settings,'demo_artist','Artist Unicode 日本語');native.obs_data_set_int(settings,'pro_preset',1);native.obs_data_set_int(settings,'pro_hue',240)
  if inScene then native.obs_data_set_bool(settings,'artwork_enabled',true);probe.playing_source_test_scene_create(settings);stage=16;ready=ticks+4
  else source=native.obs_source_create_private('obs_playing_next_development','Preset manager fixture',settings);check(source~=nil,'Create source');native.obs_source_inc_active(source);open();stage=1;ready=ticks+8 end
  native.obs_data_release(settings)
 elseif stage==16 then source=native.obs_get_source_by_name('Preset manager fixture');if source~=nil then check(true,'Create scene source on GUI thread');native.obs_source_inc_active(source);open();stage=1;ready=ticks+8 end
 elseif stage==1 and s.managerVisible==1 then check(s.managerItems==24,'Classic category');probe.playing_source_test_manager_command(1,1);stage=2;ready=ticks+4
 elseif stage==2 and s.managerItems==16 then check(true,'PRO category');probe.playing_source_test_manager_command(2,15);stage=3;ready=ticks+10
 elseif stage==3 and (s.frames or 0)>0 then
  local settings=native.obs_source_get_settings(source);check(native.obs_data_get_int(settings,'pro_preset')==16,'Apply Vertical Liquid');check(native.obs_source_get_width(source)==480 and native.obs_source_get_height(source)==720,'Preset default dimensions');check(not native.obs_data_has_user_value(settings,'pro_hue'),'Default reset left hue override');check(ffi.string(native.obs_data_get_string(settings,'provider'))=='network','Changed DJ provider');check(ffi.string(native.obs_data_get_string(settings,'receiver_profile'))==script_path()..'private-link.pcs','Changed pairing');check(ffi.string(native.obs_data_get_string(settings,'streamer_image'))==script_path()..'private-photo.png','Changed image path');check(native.obs_data_get_bool(settings,'demo_enabled') and ffi.string(native.obs_data_get_string(settings,'demo_title'))=='Manager test song','Changed demo');native.obs_data_release(settings)
  probe.playing_source_test_manager_command(3,0);stage=4;ready=ticks+4
 elseif stage==4 then probe.playing_source_test_manager_command(1,4);stage=5;ready=ticks+4
 elseif stage==5 and s.managerItems==1 then check(true,'Favorite added');probe.playing_source_test_manager_command(4,0);stage=6;ready=ticks+4
 elseif stage==6 and s.managerVisible==0 then open();stage=7;ready=ticks+4
 elseif stage==7 and s.managerVisible==1 then probe.playing_source_test_manager_command(1,4);stage=8;ready=ticks+4
 elseif stage==8 and s.managerItems==1 then check(true,'Favorite persisted on reopen');local custom=native.obs_data_create_from_json('{"text_title_font":{"face":"Arial","size":32,"flags":1},"text_title_color":4283498495,"title_outline_width":2,"artist_shadow_offset":3}');native.obs_source_update(source,custom);native.obs_data_release(custom);if directSave then probe.playing_source_test_manager_command(4,0);open(true)else probe.playing_source_test_manager_command(5,0)end;stage=13;ready=ticks+8
 elseif stage==13 and s.managerCategory==2 and s.managerItems==1 then check(true,'Save As actual source callback');probe.playing_source_test_manager_command(2,0);stage=14;ready=ticks+8
 elseif stage==14 then local settings=native.obs_source_get_settings(source);check(ffi.string(native.obs_data_get_string(settings,'preset_manager_id')):find('user:',1,true)==1,'Saved preset apply');check(native.obs_data_get_int(settings,'title_outline_width')==2 and native.obs_data_get_int(settings,'artist_shadow_offset')==3 and native.obs_data_get_int(settings,'text_title_color')==4283498495,'Save As lost customization');check(ffi.string(native.obs_data_get_string(settings,'provider'))=='network' and ffi.string(native.obs_data_get_string(settings,'receiver_profile'))==script_path()..'private-link.pcs','Saved preset changed connection');savedId=ffi.string(native.obs_data_get_string(settings,'preset_manager_id'));native.obs_data_release(settings);if fileExchange then probe.playing_source_test_manager_command(9,0);stage=19 elseif inScene then probe.playing_source_test_capture_png(source,'manager-scene.png');probe.playing_source_test_manager_command(6,0);stage=15 else probe.playing_source_test_manager_command(1,0);stage=9 end;ready=ticks+4
 elseif stage==19 then local file=io.open(script_path()..'fixture-share.obsplayingpreset','rb');if not file then probe.playing_source_test_query();return end;local bytes=file:read('*a');file:close();check(#bytes<=65536 and bytes:find('obs%-playing%-preset')~=nil,'Portable format');check(not bytes:find('receiver_profile',1,true) and not bytes:find('streamer_image',1,true) and not bytes:find('editor_mode',1,true),'Export leaked private settings');probe.playing_source_test_manager_command(10,0);stage=20;ready=ticks+6
 elseif stage==20 and s.managerCategory==3 and s.managerItems==1 then check(true,'Imported category actual file flow');probe.playing_source_test_manager_command(2,0);stage=21;ready=ticks+8
 elseif stage==21 then local settings=native.obs_source_get_settings(source);check(ffi.string(native.obs_data_get_string(settings,'preset_manager_id'))~=savedId,'Imported source reused local saved ID');check(native.obs_data_get_int(settings,'title_outline_width')==2 and native.obs_data_get_int(settings,'artist_shadow_offset')==3 and native.obs_data_get_int(settings,'text_title_color')==4283498495,'Imported appearance lost');check(ffi.string(native.obs_data_get_string(settings,'provider'))=='network' and ffi.string(native.obs_data_get_string(settings,'receiver_profile'))==script_path()..'private-link.pcs','Imported preset changed connection');native.obs_data_release(settings);if inScene then probe.playing_source_test_capture_png(source,'manager-scene.png');probe.playing_source_test_manager_command(6,0);stage=15 else probe.playing_source_test_manager_command(1,0);stage=9 end;ready=ticks+4
 elseif stage==15 and (s.captures or 0)>=1 then check((s.capturePixels or 0)>100,'Actual scene GPU image empty');check(s.artworks==1,'Applying PRO lost configured artwork fallback');probe.playing_source_test_manager_command(1,0);stage=9;ready=ticks+4
 elseif stage==9 and s.managerItems==24 then probe.playing_source_test_manager_command(2,1);stage=10;ready=ticks+10
 elseif stage==10 then local settings=native.obs_source_get_settings(source);check(native.obs_data_get_int(settings,'normal_preset')~=0 and native.obs_data_get_int(settings,'pro_preset')==0,'Classic selection');check(ffi.string(native.obs_data_get_string(settings,'provider'))=='network','Classic changed provider');removedSetting=native.obs_data_get_int(settings,'normal_preset');native.obs_data_release(settings);if keepSource then check(s.managerVisible==1 and s.consumers==1 and s.frames==1,'Live source/open manager exit setup');finish()else native.obs_source_dec_active(source);if inScene then probe.playing_source_test_scene_remove();stage=17 else native.obs_source_release(source);source=nil;probe.playing_source_test_manager_command(2,1);stage=11 end;ready=ticks+4 end
 elseif stage==17 and native.obs_source_removed(source) then check(true,'Public source marked removed while retained');probe.playing_source_test_manager_command(2,2);stage=18;ready=ticks+4
 elseif stage==18 then local settings=native.obs_source_get_settings(source);check(native.obs_data_get_int(settings,'normal_preset')==removedSetting,'Manager mutated retained removed source');native.obs_data_release(settings);native.obs_source_release(source);source=nil;stage=11;ready=ticks+4
 elseif stage==11 then check(s.managerVisible==1,'Released source crashed dialog');if keepOpen then if s.consumers==0 and s.frames==0 then check(true,'Released scene source session cleaned');finish()end else probe.playing_source_test_manager_command(4,0);stage=12;ready=ticks+4 end
 elseif stage==12 and s.managerVisible==0 then check(s.consumers==0 and s.frames==0,'Released source retained session');finish()
 end
 if probe and not done then if directSave and stage==13 then probe.playing_source_test_manager_command(8,0)end;probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,100)end
function script_unload()obs.timer_remove(pulse);if source then if stage~=17 and stage~=18 then native.obs_source_dec_active(source)end;native.obs_source_release(source);source=nil end end
