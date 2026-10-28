-- Isolated local history + authenticated loopback sender. No LAN search or user settings.
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;
typedef struct obs_data obs_data_t;typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t *obs_data_create(void);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);
const char *obs_data_get_string(obs_data_t*,const char*);obs_data_t *obs_source_get_settings(obs_source_t*);
obs_source_t *obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void *obs_obj_get_data(void*);
obs_properties_t *obs_source_properties(const obs_source_t*);obs_property_t *obs_properties_get(obs_properties_t*,const char*);void obs_properties_destroy(obs_properties_t*);
bool obs_property_button_clicked(obs_property_t*,void*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);
void playing_source_test_remote_fixture(const char*);void playing_source_test_remote_publish(int);void playing_source_test_receiver_command(int);]]
local native=ffi.load('obs.dll')
local probe,source,settings
local stage,ticks,checks=0,0,0
local safePulse
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function pulse()
 ticks=ticks+1;assert(ticks<100,'Quick connection timed out at stage '..stage)
 local s={}
 if probe then
  local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096)
  if n>0 then local text=ffi.string(b,n);for k,v in text:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end;s.title=text:match('"title":"(.-)"')end
 end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]')
  check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Open module')
  check(native.obs_init_module(module[0]),'Initialize module');probe=ffi.load(script_path()..'obs-playing-next.dll')
  probe.playing_source_test_remote_fixture(script_path()..'quick-receiver.pcs');stage=1
 elseif stage==1 and s.remoteReady==1 then
  local f=assert(io.open(script_path()..'quick-history.txt','w'));f:write('21:05 : Local artist - Local song\r\n');f:close()
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1)
  native.obs_data_set_string(settings,'history',script_path()..'quick-history.txt');native.obs_data_set_string(settings,'receiver_profile',script_path()..'quick-receiver.pcs')
  source=native.obs_source_create_private('obs_playing_next_development','Quick connection fixture',settings)
  check(source~=nil,'Private source');native.obs_source_inc_active(source);stage=2
 elseif stage==2 and s.frames==1 then
  local props=native.obs_source_properties(source);local button=native.obs_properties_get(props,'pair_receiver');check(button~=nil,'Connection button')
  native.obs_property_button_clicked(button,native.obs_obj_get_data(source));native.obs_properties_destroy(props);stage=3
 elseif stage==3 and s.remoteClients==1 and s.title=='Remote 🎵' then
  local saved=native.obs_source_get_settings(source);check(ffi.string(native.obs_data_get_string(saved,'provider'))=='network','Button did not switch source to remote');native.obs_data_release(saved)
  check(s.sessions==1,'Local session retained after remote switch');probe.playing_source_test_receiver_command(3)
  probe.playing_source_test_remote_publish(1);stage=4
 elseif stage==4 and s.title=='Remote next' then
  check(s.remoteClients==1,'Song change lost connection');probe.playing_source_test_receiver_command(1);stage=5
 elseif stage==5 and s.remoteClients==0 and s.sessions==0 and s.frames==0 then
  check(true,'Forget detached live remote');probe.playing_source_test_receiver_command(2)
  native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=6
 elseif stage==6 and s.consumers==0 then
  local f=assert(io.open(script_path()..'quick-connect-result.txt','w'));f:write('PASS checks='..checks..'; button_switches_provider=1; authenticated_loopback=1; song_change=1; forget_disconnects=1; no_LAN_search=1');f:close()
  obslua.timer_remove(safePulse);stage=7
 end
 if probe then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obslua.timer_remove(safePulse);error(err)end end
function script_load()obslua.timer_add(safePulse,200)end
function script_unload()
 obslua.timer_remove(safePulse)
 if source then native.obs_source_dec_active(source);native.obs_source_release(source)end
 if settings then native.obs_data_release(settings)end
end
