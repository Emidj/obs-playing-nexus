-- Private browser sources only in owned portable; no original reader timers.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);
void obs_data_set_bool(obs_data_t*,const char*,bool);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_inc_showing(obs_source_t*);void obs_source_dec_showing(obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);int playing_source_test_read(char*,int);]]
local native=ffi.load('obs.dll')
local probe,sources=nil,{}
local ticks,stage,index,matte,requested=0,0,1,1,0
local function count()
 local buffer=ffi.new('char[4096]');local size=probe.playing_source_test_read(buffer,4096)
 return size>0 and tonumber(ffi.string(buffer,size):match('"captures":(%d+)')) or 0
end
local function release()
 for _,item in ipairs(sources)do native.obs_source_dec_showing(item.source);native.obs_source_dec_active(item.source);native.obs_source_release(item.source)end
 sources={}
end
local function run()
 ticks=ticks+1;assert(ticks<200,'CEF glyph capture timeout')
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');assert(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0);assert(native.obs_init_module(module[0]))
  probe=ffi.load(script_path()..'obs-playing-next.dll')
  local file=assert(io.open(script_path()..'glyph-reference-cases.json','rb'));local json=file:read('*a');file:close()
  local document=obs.obs_data_create_from_json(json);assert(document~=nil)
  local cases=obs.obs_data_get_array(document,'cases');local caseCount=obs.obs_data_array_count(cases);assert(caseCount>=12 and caseCount<=32)
  for i=0,caseCount-1 do
   -- Never pass SWIG userdata to the FFI hook. Own actual C pointers using
   -- the same native creation/release convention as the proven GDI harness.
   local row=obs.obs_data_array_item(cases,i);local settings=native.obs_data_create()
   native.obs_data_set_bool(settings,'is_local_file',true);native.obs_data_set_string(settings,'local_file',obs.obs_data_get_string(row,'localFile'))
   native.obs_data_set_string(settings,'css','');native.obs_data_set_int(settings,'width',obs.obs_data_get_int(row,'width'));native.obs_data_set_int(settings,'height',obs.obs_data_get_int(row,'captureHeight'))
   native.obs_data_set_bool(settings,'shutdown',false);native.obs_data_set_bool(settings,'restart_when_active',false);native.obs_data_set_bool(settings,'reroute_audio',false);native.obs_data_set_int(settings,'webpage_control_level',0)
   local id=obs.obs_data_get_string(row,'id');local source=native.obs_source_create_private('browser_source','CEF glyph '..id,settings);assert(source~=nil)
   native.obs_source_inc_active(source);native.obs_source_inc_showing(source);sources[#sources+1]={source=source,id=id}
   native.obs_data_release(settings);obs.obs_data_release(row)
  end
  obs.obs_data_array_release(cases);obs.obs_data_release(document);stage=1
 elseif stage==1 and ticks>=60 then stage=2
 elseif stage==2 then
  local item=sources[index];local suffix=matte==1 and '-transparent.png'or matte==2 and '-black.png'or'-white.png'
  assert(ffi.istype('obs_source_t*',item.source),'Capture requires native C pointer, not Lua userdata')
  probe.playing_source_test_capture_png(item.source,item.id..suffix);requested=requested+1;stage=3
 elseif stage==3 and count()>=requested then
  matte=matte+1;if matte>3 then matte=1;index=index+1 end
  if index<=#sources then stage=2 else
   local captured=index-1;release();local file=assert(io.open(script_path()..'glyph-reference-result.txt','wb'));file:write('PASS '..(captured*3)..' OBS CEF glyph captures; readiness/pixels must be checked separately');file:close();obs.timer_remove(run);stage=4
  end
 end
end
function script_load()obs.timer_add(run,100)end
function script_unload()obs.timer_remove(run);release()end
