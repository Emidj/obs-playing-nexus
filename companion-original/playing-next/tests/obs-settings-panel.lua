-- Native property contract for the user-selected single editor; no test hooks.
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t *obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);void obs_data_set_int(obs_data_t*,const char*,int64_t);
void obs_data_set_bool(obs_data_t*,const char*,bool);int64_t obs_data_get_int(obs_data_t*,const char*);const char *obs_data_get_string(obs_data_t*,const char*);
obs_data_t *obs_source_get_settings(obs_source_t*);obs_source_t *obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
obs_properties_t *obs_source_properties(const obs_source_t*);obs_property_t *obs_properties_get(obs_properties_t*,const char*);
obs_property_t *obs_properties_first(obs_properties_t*);bool obs_property_next(obs_property_t**);const char *obs_property_name(obs_property_t*);
bool obs_property_visible(obs_property_t*);void obs_properties_apply_settings(obs_properties_t*,obs_data_t*);void obs_properties_destroy(obs_properties_t*);
size_t obs_property_list_item_count(obs_property_t*);int64_t obs_property_list_item_int(obs_property_t*,size_t);
]]
local native=ffi.load('obs.dll')
local source,settings,props
local checks=0
local function check(ok,why)checks=checks+1;assert(ok,why)end
local allowed={}
for _,key in ipairs({'pair_receiver','show_existing','artwork_enabled','artwork_fallback','twitch_profile_auto','chat_enabled','chat_connect','chat_message','chat_delay','provider','mode','preset_family','preset_style',
 'pro_hue','pro_light_intensity','pro_light_pulse','pro_panel_opacity','animation_speed','normal_transition','normal_duration',
 'hide_seconds','title_overflow','artist_overflow','canvas_width','canvas_height','text_title_font','text_artist_font','text_title_color','text_artist_color',
 'text_offset_x','text_offset_y','title_align','artist_align','demo_enabled','demo_previous','demo_next','nexus_credit','status'})do allowed[key]=true end
local first={'pair_receiver','show_existing','artwork_enabled','artwork_fallback','twitch_profile_auto'}
local function visible(key)return native.obs_property_visible(native.obs_properties_get(props,key))end
local function visibleKeys()
 local values={};local property=ffi.new('obs_property_t*[1]',native.obs_properties_first(props))
 while property[0]~=nil do
  if native.obs_property_visible(property[0])then local name=ffi.string(native.obs_property_name(property[0]));check(allowed[name],'Unselected control visible: '..name);values[#values+1]=name end
  native.obs_property_next(property)
 end
 return values
end
local function run()
 obslua.timer_remove(run)
 local ok,err=pcall(function()
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init')
  settings=native.obs_data_create_from_json('{"provider":"serato","mode":0,"preset_family":1,"preset_style":1,"pro_preset":1,"normal_preset":0,"canvas_width":960,"canvas_height":367,"demo_enabled":false,"text_offset_x":8,"text_offset_y":18,"pro_hue":35,"pro_light_intensity":65,"title_outline_width":3,"streamer_image":"C:/saved-custom-cover.png"}')
  source=native.obs_source_create_private('obs_playing_next_development','Single settings editor',settings);check(source~=nil,'Source')
  props=native.obs_source_properties(source);check(native.obs_properties_get(props,'editor_mode')==nil,'Three-mode selector remains')
  check(visible('chat_enabled') and not visible('chat_connect') and not visible('chat_message'),'Chat controls not opt-in')
  native.obs_data_set_bool(settings,'chat_enabled',true);native.obs_properties_apply_settings(props,settings)
  check(visible('chat_connect') and visible('chat_message') and visible('chat_delay'),'Chat configuration hidden after enabling')
  native.obs_data_set_bool(settings,'chat_enabled',false);native.obs_properties_apply_settings(props,settings)
  local baseline
  for _,oldMode in ipairs({0,1,2,0})do
   native.obs_data_set_int(settings,'editor_mode',oldMode);native.obs_properties_apply_settings(props,settings)
   local keys=visibleKeys();for i,key in ipairs(first)do check(keys[i]==key,'Connection/image block is not first: '..key)end
   local signature=table.concat(keys,',');if baseline then check(signature==baseline,'Saved editor mode still changes visible controls')else baseline=signature end
   for key in pairs(allowed)do if key~='status' and key~='chat_connect' and key~='chat_message' and key~='chat_delay' then check(visible(key),'Selected PRO control hidden: '..key)end end
   check(native.obs_data_get_int(settings,'text_offset_x')==8 and native.obs_data_get_int(settings,'text_offset_y')==18,'Saved text position changed')
   check(native.obs_data_get_int(settings,'title_outline_width')==3 and ffi.string(native.obs_data_get_string(settings,'streamer_image'))=='C:/saved-custom-cover.png','Hidden saved settings were discarded')
   for _,key in ipairs({'title_overflow','artist_overflow'})do local list=native.obs_properties_get(props,key);check(native.obs_property_list_item_count(list)==5,'Long-text modes lost');check(native.obs_property_list_item_int(list,4)==4,'Ping-pong lost')end
  end
  -- Style changes may hide controls that do not apply, but cannot expose old editors.
  for style=1,16 do native.obs_data_set_int(settings,'pro_preset',style);native.obs_data_set_int(settings,'preset_style',style);native.obs_properties_apply_settings(props,settings);visibleKeys();check(not visible('text_cover_size'),'Manual cover-size control returned')end
  native.obs_data_set_int(settings,'pro_preset',0);native.obs_data_set_int(settings,'normal_preset',1);native.obs_data_set_int(settings,'preset_family',0);native.obs_data_set_int(settings,'preset_style',1);native.obs_properties_apply_settings(props,settings);visibleKeys()
  check(not visible('artwork_enabled') and not visible('artwork_fallback'),'Classic style gained artwork')
  check(visible('text_title_font') and visible('text_artist_color') and visible('normal_transition'),'Classic text/transition controls hidden')
  local f=assert(io.open(script_path()..'settings-panel-result.txt','w'));f:write('PASS checks='..checks..'; one_editor=1; top_block=1; only_selected_controls=1; legacy_values_preserved=1; PRO_styles=16; classic_no_artwork=1; ping_pong=1; hooks=OFF\nPRO order: '..baseline);f:close()
 end)
 if props then native.obs_properties_destroy(props);props=nil end
 if source then native.obs_source_release(source);source=nil end
 if settings then native.obs_data_release(settings);settings=nil end
 if not ok then error(err)end
end
function script_load()obslua.timer_add(run,800)end
function script_unload()obslua.timer_remove(run)end
