-- Isolated reproduction control for the phase25 font/settings crash.
local ffi=require('ffi');local obs=obslua
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);obs_data_t* obs_data_get_default_obj(obs_data_t*,const char*);int64_t obs_data_get_int(obs_data_t*,const char*);const char* obs_data_get_string(obs_data_t*,const char*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);]]
local native=ffi.load('obs.dll');local probe,data;local sources={};local ticks,stage=0,0;local safePulse
local function pulse()
 ticks=ticks+1;assert(ticks<150,'Shared settings control timeout')
 if stage==0 then
  stage=-1 -- Never initialize the module again following a failure.
  local m=ffi.new('obs_module_t*[1]');assert(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0);assert(native.obs_init_module(m[0]));probe=ffi.load(script_path()..'obs-playing-next.dll')
  data=native.obs_data_create_from_json('{"provider":"network","demo_enabled":true,"demo_title":"Shared basic","demo_artist":"Font defaults","normal_preset":0,"pro_preset":0}')
  local font
  for i=1,2 do
   sources[i]=native.obs_source_create_private('obs_playing_next_development','Basic shared '..i,data);assert(sources[i]~=nil);native.obs_source_inc_active(sources[i])
   local current=native.obs_data_get_default_obj(data,'text_title_font');assert(current~=nil,'Default font missing')
   assert(native.obs_data_get_int(current,'size')==30 and ffi.string(native.obs_data_get_string(current,'face'))=='Segoe UI','Default font corrupted')
   if font then assert(font==current,'Repeated defaults replaced shared font');native.obs_data_release(current);native.obs_data_release(font);font=nil else font=current end
  end
  stage=1
 else
  local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);local raw=n>0 and ffi.string(b,n) or ''
  if stage==1 and raw:find('"frames":2',1,true) then probe.playing_source_test_capture_png(sources[1],'basic-shared.png');stage=2
  elseif stage==2 and raw:find('"captures":1',1,true) then
   local pixels=tonumber(raw:match('"capturePixels":(%d+)'));assert(pixels and pixels>100,'Basic source drew no text')
   for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={};native.obs_data_release(data);data=nil;stage=3
  elseif stage==3 and raw:find('"frames":0',1,true) and raw:find('"sessions":0',1,true) then
   local f=assert(io.open(script_path()..'source-render-result.txt','w'));f:write('PASS basic shared settings; two default-font sources; visible text; normal teardown');f:close();obs.timer_remove(safePulse);stage=4
  end
 end
 if probe then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obs.timer_remove(safePulse);error(err)end end
function script_load()obs.timer_add(safePulse,100)end
function script_unload()obs.timer_remove(safePulse);for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;if data then native.obs_data_release(data)end end
