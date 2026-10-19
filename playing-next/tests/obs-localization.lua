-- Real module properties, no hooks, no program readers or network listeners.
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t *obs_data_create(void);void obs_data_set_bool(obs_data_t*,const char*,bool);void obs_data_release(obs_data_t*);
const char *obs_data_get_json(obs_data_t*);obs_data_t *obs_source_get_settings(obs_source_t*);
obs_source_t *obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
obs_properties_t *obs_source_properties(const obs_source_t*);obs_property_t *obs_properties_get(obs_properties_t*,const char*);
obs_property_t *obs_properties_first(obs_properties_t*);bool obs_property_next(obs_property_t**);const char *obs_property_description(obs_property_t*);
void obs_properties_destroy(obs_properties_t*);size_t obs_property_list_item_count(obs_property_t*);const char *obs_property_list_item_name(obs_property_t*,size_t);
void obs_module_set_locale(const char*);]]
local native=ffi.load('obs.dll')
local probe,source,settings,snapshot
local stage,checks=0,0
local pulse
local languages={'es-ES','en-US','it-IT','fr-FR','de-DE'}
local expected={'Conectar Companion','Connect Companion','Connetti Companion','Connecter Companion','Companion verbinden'}
local program={'Programa DJ','DJ application','Programma DJ','Logiciel DJ','DJ-Programm'}
local function check(ok,why)checks=checks+1;assert(ok,why)end
pulse=function()
 local ok,err=pcall(function()
  if stage==0 then
   local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Open module')
   check(native.obs_init_module(module[0]),'Initialize module');probe=ffi.load(script_path()..'obs-playing-next.dll')
   settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true)
   source=native.obs_source_create_private('obs_playing_next_development','Localization fixture',settings);check(source~=nil,'Private source')
   local props=native.obs_source_properties(source);native.obs_properties_destroy(props)
   local saved=native.obs_source_get_settings(source);snapshot=ffi.string(native.obs_data_get_json(saved));native.obs_data_release(saved);stage=1
  elseif stage<=5 then
   probe.obs_module_set_locale(languages[stage]);local props=native.obs_source_properties(source)
   check(ffi.string(native.obs_property_description(native.obs_properties_get(props,'pair_receiver')))==expected[stage],'Connection button not translated: '..languages[stage])
   check(ffi.string(native.obs_property_description(native.obs_properties_get(props,'provider')))==program[stage],'DJ selector not translated: '..languages[stage])
   local list=native.obs_properties_get(props,'pro_preset')
   for i=0,tonumber(native.obs_property_list_item_count(list))-1 do check(not ffi.string(native.obs_property_list_item_name(list,i)):find('en desarrollo',1,true),'Development label remains')end
   native.obs_properties_destroy(props)
   local saved=native.obs_source_get_settings(source);local after=ffi.string(native.obs_data_get_json(saved))
   if after~=snapshot then local f=assert(io.open(script_path()..'settings-before.json','w'));f:write(snapshot);f:close();f=assert(io.open(script_path()..'settings-after.json','w'));f:write(after);f:close()end
   check(after==snapshot,'Language changed saved settings');native.obs_data_release(saved);stage=stage+1
  else
   native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil
   local f=assert(io.open(script_path()..'localization-result.txt','w'));f:write('PASS checks='..checks..'; languages=es,en,it,fr,de; real_module_properties=1; settings_unchanged=1; development_labels=0; hooks=OFF');f:close()
   obslua.timer_remove(pulse)
  end
 end)
 if not ok then obslua.timer_remove(pulse);error(err)end
end
function script_load()obslua.timer_add(pulse,200)end
function script_unload()obslua.timer_remove(pulse);if source then native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
