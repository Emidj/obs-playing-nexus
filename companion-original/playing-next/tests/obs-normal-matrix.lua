-- Actual development source: eighteen original GDI cases, transparent/opaque mattes.
-- Each fresh source is captured during its ten-second initial scroll pause.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);
obs_data_t* obs_data_create_from_json(const char*);const char* obs_data_get_string(obs_data_t*,const char*);const char* obs_data_get_json(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll')
local probe,source=nil,nil
local cases={}
for preset,id in ipairs({'studio','minimal'})do for _,width in ipairs({400,720,1600})do
 for number,track in ipairs({{'Lipstick','Alaia & Gallo'},{'Jóga 🎵 日本語','Björk'},
  {'Long native title '..string.rep('scrolling ',20),'Long artist '..string.rep('overflow ',12)}})do
  cases[#cases+1]={preset=preset,width=width,title=track[1]:gsub('%s+$',''),artist=track[2]:gsub('%s+$',''),name=id..'-'..width..'-'..number}
 end
end end
local stage,index,ticks,checks,captures,matte=0,1,0,0,0,0
local created=0
local inputs={}
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function read()
 local buffer=ffi.new('char[4096]');local n=probe.playing_source_test_read(buffer,4096)
 local text=n>0 and ffi.string(buffer,n)or'';local s={}
 for key,value in text:gmatch('"([%w]+)":(%d+)')do s[key]=tonumber(value)end
 if n>0 then local data=native.obs_data_create_from_json(text);check(data~=nil,'Evidence JSON')
  s.title=ffi.string(native.obs_data_get_string(data,'title'));s.artist=ffi.string(native.obs_data_get_string(data,'artist'));native.obs_data_release(data)
 end;return s
end
local function request()
 local suffix=matte==0 and '.png'or matte==1 and '-black.png'or'-white.png'
 probe.playing_source_test_capture_png(source,cases[index].name..suffix);captures=captures+1
end
local function pulse()
 ticks=ticks+1;check(ticks<220,'Native matrix timeout at '..stage..' case '..index)
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init')
  probe=ffi.load(script_path()..'obs-playing-next.dll');stage=1
 elseif stage==1 then
  local item=cases[index];local history=script_path()..'matrix-history-'..index..'.txt'
  local file=assert(io.open(history,'wb'));file:write('21:05 : '..item.artist..' - '..item.title..'\r\n');file:close()
  local settings=native.obs_data_create()
  native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1)
  native.obs_data_set_string(settings,'history',history);native.obs_data_set_int(settings,'normal_preset',item.preset)
  native.obs_data_set_int(settings,'canvas_width',item.width);native.obs_data_set_int(settings,'normal_pause',10)
  source=native.obs_source_create_private('obs_playing_next_development',item.name,settings);native.obs_data_release(settings)
  check(source~=nil,'Actual native source');native.obs_source_inc_active(source);created=ticks;stage=2
 elseif stage==2 then
  local s=read();if s.frames==1 then
   local item=cases[index];check(s.title==item.title and s.artist==item.artist,'Actual provider metadata differs from reference input')
   local record=native.obs_data_create();native.obs_data_set_string(record,'name',item.name)
   native.obs_data_set_string(record,'title',s.title);native.obs_data_set_string(record,'artist',s.artist)
   inputs[#inputs+1]=ffi.string(native.obs_data_get_json(record));native.obs_data_release(record)
   matte=0;request();stage=3
  end
 elseif stage==3 then
  local s=read();if s.captures==captures then
   check(ticks-created<90,'Capture escaped initial pause');check(s.capturePixels>0,'Native matrix empty capture')
   matte=matte+1
   if matte<3 then request()else
    native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;stage=4
   end
  end
 elseif stage==4 then
  local s=read();if s.frames==0 and s.sessions==0 then
   index=index+1
   if index<=#cases then stage=1 else
    check(captures==54,'Native capture count')
    local file=assert(io.open(script_path()..'normal-matrix-result.txt','wb'))
    file:write('PASS '..checks..' native matrix checks; cases=18; captures=54; initial_pause=10; pixel_gate_external=1');file:close()
    file=assert(io.open(script_path()..'normal-matrix-inputs.json','wb'));file:write('['..table.concat(inputs,',')..']\n');file:close()
    stage=5;obs.timer_remove(pulse)
   end
  end
 end
 if probe and stage<5 then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,100)end
function script_unload()obs.timer_remove(pulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil end end
