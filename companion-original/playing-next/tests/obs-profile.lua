-- Isolated profiling fixture. Production Playing DLL; native RAW sink only.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;typedef struct obs_scene obs_scene_t;typedef struct obs_scene_item obs_sceneitem_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_update(obs_source_t*,obs_data_t*);bool obs_source_active(const obs_source_t*);
obs_scene_t* obs_scene_create_private(const char*);obs_source_t* obs_scene_get_source(const obs_scene_t*);void obs_scene_release(obs_scene_t*);obs_sceneitem_t* obs_scene_add(obs_scene_t*,obs_source_t*);void obs_sceneitem_remove(obs_sceneitem_t*);bool obs_sceneitem_set_visible(obs_sceneitem_t*,bool);
struct vec2 {float x,y;};void obs_sceneitem_set_pos(obs_sceneitem_t*,const struct vec2*);void obs_sceneitem_set_scale(obs_sceneitem_t*,const struct vec2*);
obs_source_t* obs_get_output_source(uint32_t);void obs_set_output_source(uint32_t,obs_source_t*);uint64_t os_gettime_ns(void);
bool playing_profile_start(void);void playing_profile_stop(void);int playing_profile_read(char*,int);
void* obs_frontend_get_main_window_handle(void);int PostMessageW(void*,unsigned int,uintptr_t,intptr_t);
]]
local native=ffi.load('obs.dll');local frontend=ffi.load('obs-frontend-api.dll');local user=ffi.load('user32');local sink;local scene,original;local sources={};local items={};local cases={};local index,phase,start=0,'start',0;local loaded=false;local finished=false;local ticks=0;local config;local lastChange=0;local safePulse
local function write(name,text)local f=assert(io.open(script_path()..name,'wb'));f:write(text);f:close()end
local function read(name)local f=assert(io.open(script_path()..name,'rb'));local t=f:read('*a');f:close();return t end
local function clear()
 for _,item in ipairs(items)do native.obs_sceneitem_remove(item)end;items={}
 for _,source in ipairs(sources)do native.obs_source_release(source)end;sources={}
end
local function mark(tag)
 local c=cases[index];write(string.format('phase-%03d-%s.json',index,tag),string.format('{"index":%d,"name":"%s","phase":"%s","startNs":%.0f}',index,c.name,tag,tonumber(native.os_gettime_ns())))
end
local function begin()
 index=index+1;if index>#cases then
  clear();native.obs_set_output_source(0,original);if original~=nil then native.obs_source_release(original);original=nil end;native.obs_scene_release(scene);scene=nil;sink.playing_profile_stop();write('profile-result.txt','PASS production scene scenarios='..#cases..'; RAW output; normal source lifecycle');finished=true;obs.timer_remove(safePulse);user.PostMessageW(frontend.obs_frontend_get_main_window_handle(),0x0010,0,0);return
 end
 clear();local c=cases[index]
 if c.kind~='baseline' and not loaded then local m=ffi.new('obs_module_t*[1]');assert(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');assert(native.obs_init_module(m[0]),'Module init');loaded=true end
 if c.kind=='history' then write('profile-history.txt','21:00 : Artist - Profile initial\r\n')elseif c.kind=='waiting' then write('profile-history.txt','')end
 if c.kind~='baseline' and c.kind~='recovery' then
  for i=1,c.count do
   local style=(c.kind=='static' or c.kind=='idle' or c.kind=='history' or c.kind=='waiting') and 1 or c.kind=='mixed' and ({1,4,6,8,10,16})[i] or 4
   local history=c.kind=='history' or c.kind=='waiting'
   local settings=string.format('{"provider":"%s","mode":1,"demo_enabled":%s,"demo_title":"Profile Track","demo_artist":"Artist","demo_image":"%sprofile-artwork.png","streamer_image":"%sprofile-artwork.png","artwork_enabled":%s,"history":"%sprofile-history.txt","pro_preset":%d,"pro_light_intensity":80,"pro_light_pulse":false,"performance_profile":%d,"normal_duration":200}',history and 'virtualdj' or 'network',history and 'false' or 'true',script_path(),script_path(),c.artwork and 'true' or 'false',script_path(),style,c.profile)
   local d=native.obs_data_create_from_json(settings);assert(d~=nil,'Settings');local source=native.obs_source_create_private('obs_playing_next_development','Profile '..i,d);native.obs_data_release(d);assert(source~=nil,'Source create');sources[i]=source
   if c.kind~='idle' then local item=native.obs_scene_add(scene,source);assert(item~=nil,'Scene item');items[i]=item;if c.count>1 then local scale=ffi.new('struct vec2',{.4,.4});local pos=ffi.new('struct vec2',{((i-1)%2)*580,math.floor((i-1)/2)*210});native.obs_sceneitem_set_scale(item,scale);native.obs_sceneitem_set_pos(item,pos)end end
  end
 end
 phase='warmup';start=tonumber(native.os_gettime_ns())/1e9;lastChange=start;mark('warmup')
end
local function pulse()
 ticks=ticks+1;local now=tonumber(native.os_gettime_ns())/1e9
 if phase=='start' then
  assert(read('profile-owned.flag')=='isolated fixture','Not an owned profiling runtime');config=obs.obs_data_create_from_json(read('profile-config.json'));assert(config~=nil,'Config');local a=obs.obs_data_get_array(config,'cases');for i=0,obs.obs_data_array_count(a)-1 do local d=obs.obs_data_array_item(a,i);local c={name=obs.obs_data_get_string(d,'name'),kind=obs.obs_data_get_string(d,'kind'),profile=obs.obs_data_get_int(d,'profile'),count=obs.obs_data_get_int(d,'count'),warmup=obs.obs_data_get_double(d,'warmup'),seconds=obs.obs_data_get_double(d,'seconds'),changes=obs.obs_data_get_bool(d,'changes'),artwork=obs.obs_data_get_bool(d,'artwork')};assert(c.name:match('^[%w_]+$') and c.count<=6 and c.warmup>=1 and c.seconds>=1,'Case limits');cases[#cases+1]=c;obs.obs_data_release(d)end;obs.obs_data_array_release(a);obs.obs_data_release(config);config=nil
  sink=ffi.load(script_path()..'playing-obs-profile-probe.dll');scene=native.obs_scene_create_private('Profiling RAW fixture');assert(scene~=nil,'Scene create');original=native.obs_get_output_source(0);native.obs_set_output_source(0,native.obs_scene_get_source(scene));assert(sink.playing_profile_start(),'NV12 RAW output required');begin()
 elseif not finished then
  local c=cases[index]
  if phase=='warmup' and now-start>=c.warmup then
   for _,s in ipairs(sources)do assert(native.obs_source_active(s)==(c.kind~='idle'),'Activity mismatch')end;phase='measure';start=now;mark('measure')
  elseif phase=='measure' and now-start>=c.seconds then mark('end');begin()
  elseif c.changes and now-lastChange>=2 then
   if c.kind=='history' then write('profile-history.txt','21:00 : Artist - Profile '..ticks..'\r\n')else for _,source in ipairs(sources)do local d=native.obs_data_create_from_json('{"demo_title":"Profile change '..ticks..'"}');native.obs_source_update(source,d);native.obs_data_release(d)end end;lastChange=now
  end
 end
 if sink and not finished and ticks%10==0 then local b=ffi.new('char[512]');local n=sink.playing_profile_read(b,512);assert(n>0,'Observer');write(string.format('video-%06d.json',ticks),string.format('{"index":%d,"phase":"%s","elapsedNs":%.0f,"video":%s}',index,phase,tonumber(native.os_gettime_ns()),ffi.string(b,n)))end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obs.timer_remove(safePulse);write('profile-error.txt',tostring(err));error(err)end end
function script_load()obs.timer_add(safePulse,100)end
function script_unload()obs.timer_remove(safePulse);clear();if scene then native.obs_set_output_source(0,original);if original~=nil then native.obs_source_release(original)end;native.obs_scene_release(scene);scene=nil end;if sink then sink.playing_profile_stop()end end
