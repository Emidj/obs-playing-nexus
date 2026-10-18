-- Native source latency: fixture input -> >=5% title opacity render -> matching RAW frame.
local obs=obslua;local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;typedef struct obs_scene obs_scene_t;typedef struct obs_scene_item obs_sceneitem_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);obs_scene_t* obs_scene_create_private(const char*);obs_source_t* obs_scene_get_source(const obs_scene_t*);void obs_scene_release(obs_scene_t*);obs_sceneitem_t* obs_scene_add(obs_scene_t*,obs_source_t*);void obs_sceneitem_remove(obs_sceneitem_t*);
obs_source_t* obs_get_output_source(uint32_t);void obs_set_output_source(uint32_t,obs_source_t*);uint64_t os_gettime_ns(void);
bool playing_profile_start(void);void playing_profile_stop(void);uint64_t playing_profile_delivery(uint64_t);
void playing_source_test_profile_trace(obs_source_t*,bool);void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_remote_fixture(const char*);void playing_source_test_remote_publish(int);
void* obs_frontend_get_main_window_handle(void);int PostMessageW(void*,unsigned int,uintptr_t,intptr_t);
]]
local native=ffi.load('obs.dll');local frontend=ffi.load('obs-frontend-api.dll');local user=ffi.load('user32');local probe,sink,scene,original,source,item;local cases={{name='history_eco',profile=0},{name='history_balanced',profile=1},{name='history_maximum',profile=2},{name='tls_loopback',profile=1,remote=true}}
local index=0;local state='init';local count=0;local target;local commit=0;local due=0;local rows={};local checks=0;local samples=20;local safePulse;local ticks=0
local function write(name,text)local f=assert(io.open(script_path()..name,'wb'));f:write(text);f:close()end
local function check(v,why)checks=checks+1;assert(v,why)end
local function observed()
 if not probe then return nil,{} end;local b=ffi.new('char[32768]');local n=probe.playing_source_test_read(b,32768);if n==0 then return nil,{}end
 local d=obs.obs_data_create_from_json(ffi.string(b,n));local result={ready=obs.obs_data_get_int(d,'remoteReady'),clients=obs.obs_data_get_int(d,'remoteClients')};local list={};local a=obs.obs_data_get_array(d,'profileEvents');if a then for i=0,obs.obs_data_array_count(a)-1 do local e=obs.obs_data_array_item(a,i);list[#list+1]={title=obs.obs_data_get_string(e,'title'),render=tonumber(obs.obs_data_get_string(e,'renderNs')),video=tonumber(obs.obs_data_get_string(e,'videoNs'))};obs.obs_data_release(e)end;obs.obs_data_array_release(a)end;obs.obs_data_release(d);return result,list
end
local function remove()
 if item then native.obs_sceneitem_remove(item);item=nil end;if source then probe.playing_source_test_profile_trace(source,false);native.obs_source_release(source);source=nil end
end
local function nextCase()
 remove();index=index+1;if index>#cases then
  sink.playing_profile_stop();native.obs_set_output_source(0,original);if original~=nil then native.obs_source_release(original);original=nil end;native.obs_scene_release(scene);scene=nil
  write('latency-samples.json','['..table.concat(rows,',')..']');write('latency-result.txt','PASS '..checks..' checks; history profiles=3; TLS loopback=1; samples='..#rows..'; >=5% title opacity; RAW timestamp correlation');state='done';obs.timer_remove(safePulse);user.PostMessageW(frontend.obs_frontend_get_main_window_handle(),0x0010,0,0);return
 end
 count=0;local c=cases[index]
 if c.remote then probe.playing_source_test_remote_fixture(script_path()..'latency-private.pcs');state='remote_setup';due=tonumber(native.os_gettime_ns())+8e9
 else target='Lat initial '..c.profile;write('latency-history.txt','21:00 : Artist - '..target..'\r\n');commit=tonumber(native.os_gettime_ns());state='create' end
end
local function create()
 local c=cases[index];local settings=string.format('{"provider":"%s","mode":1,"history":"%slatency-history.txt","receiver_profile":"%slatency-private.pcs","pro_preset":1,"artwork_enabled":false,"performance_profile":%d,"normal_duration":200}',c.remote and 'network' or 'virtualdj',script_path(),script_path(),c.profile)
 local d=native.obs_data_create_from_json(settings);source=native.obs_source_create_private('obs_playing_next_development','Latency',d);native.obs_data_release(d);check(source~=nil,'Create latency source');probe.playing_source_test_profile_trace(source,true);item=native.obs_scene_add(scene,source);check(item~=nil,'Scene item');state='await';due=commit+5e9
end
local function pulse()
 ticks=ticks+1;local now=tonumber(native.os_gettime_ns());check(ticks<25000,'Latency overall deadline')
 if state=='init' then
  local flag=assert(io.open(script_path()..'profile-owned.flag','r'));check(flag:read('*a')=='isolated fixture','Owned runtime');flag:close()
  local config=io.open(script_path()..'latency-count.txt','r');if config then samples=tonumber(config:read('*a'));config:close();check(samples>=2 and samples<=20,'Sample limit')end
  local m=ffi.new('obs_module_t*[1]');check(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(m[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll');sink=ffi.load(script_path()..'playing-obs-profile-probe.dll');scene=native.obs_scene_create_private('Latency RAW');original=native.obs_get_output_source(0);native.obs_set_output_source(0,native.obs_scene_get_source(scene));check(sink.playing_profile_start(),'RAW NV12');nextCase()
 elseif state=='create' then create()
 elseif state=='remote_setup' then local s=observed();check(now<due,'Remote fixture deadline');if s and s.ready==1 then target='Remote 🎵';commit=now;create()end
 elseif state=='await' then
  check(now<due,'Frame delivery deadline '..tostring(target));local _,events=observed()
  for _,e in ipairs(events)do if e.title==target and e.render>=commit then local delivered=tonumber(sink.playing_profile_delivery(e.video));if delivered>0 then
   check(delivered>=e.render and e.render>=commit,'Clock/order mismatch');rows[#rows+1]=string.format('{"scenario":"%s","sample":%d,"inputNs":"%.0f","renderNs":"%.0f","videoNs":"%.0f","deliveryNs":"%.0f","renderMs":%.6f,"rawMs":%.6f}',cases[index].name,count,commit,e.render,e.video,delivered,(e.render-commit)/1e6,(delivered-commit)/1e6)
   if count>=samples then nextCase()else state='wait';due=now+2e9 end;break
  end end end
 elseif state=='wait' and now>=due then
  count=count+1;local c=cases[index]
  if c.remote then target=count%2==1 and 'Remote next' or 'Remote resumed';commit=now;probe.playing_source_test_remote_publish(count%2==1 and 1 or 3)
  else target=string.format('Lat %d %02d',c.profile,count);write('latency-history.txt','21:00 : Artist - '..target..'\r\n');commit=tonumber(native.os_gettime_ns())end
  state='await';due=commit+5e9
 end
 if probe and state~='done' then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obs.timer_remove(safePulse);write('latency-error.txt',tostring(err));error(err)end end
function script_load()obs.timer_add(safePulse,10)end
function script_unload()obs.timer_remove(safePulse);remove();if scene then native.obs_set_output_source(0,original);if original~=nil then native.obs_source_release(original)end;native.obs_scene_release(scene)end;if sink then sink.playing_profile_stop()end end
