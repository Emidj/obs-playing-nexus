-- Original Prism/Air references only. Load the preserved source in a separate
-- environment, without calling its script_load or starting its reader timers.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
void playing_source_test_capture_png(obs_source_t*,const char*);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll')
local probe,info,sources,instances,metrics=nil,nil,{}, {}, {}
local stage,index,ticks,checks=0,1,0,0
local requested,matte=0,0
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function configureChild(child,text)
 local settings=obs.obs_source_get_settings(child);obs.obs_data_set_string(settings,'text',text);obs.obs_source_update(child,settings);obs.obs_data_release(settings)
end
local function captureCount()
 local buffer=ffi.new('char[4096]');local count=probe.playing_source_test_read(buffer,4096)
 return count>0 and tonumber(ffi.string(buffer,count):match('"captures":(%d+)')) or 0
end
local function run()
 ticks=ticks+1;check(ticks<150,'Reference timeout')
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Open probe');check(native.obs_init_module(module[0]),'Init probe')
  probe=ffi.load(script_path()..'obs-playing-next.dll')
  local base=script_path()..'../../obs-playing/'
  local proxy=setmetatable({obs_register_source=function(value)info=value end},{__index=obs})
  local environment=setmetatable({obslua=proxy,script_path=function()return base end},{__index=_G})
  local original=assert(loadfile(base..'obs-playing.lua'));setfenv(original,environment);original()
  check(info~=nil,'Original descriptor')
  local create=info.create
  local tick=info.video_tick;info.video_tick=function(d)tick(d,0)end -- Exact t=0, including long text.
  info.id='playing_normal_original_reference'
  info.create=function(settings,source)
   local d=create(settings,source);check(not d.browser,'Native original unexpectedly used Browser')
   d.track={title=obs.obs_data_get_string(settings,'reference_title'),artist=obs.obs_data_get_string(settings,'reference_artist')};d.visible=true;d.hidden=false
   configureChild(d.children.label,'NOW PLAYING');configureChild(d.children.title,d.track.title);configureChild(d.children.artist,d.track.artist)
   instances[obs.obs_source_get_name(source)]=d;return d
  end
  obs.obs_register_source(info)
  for _,preset in ipairs({'studio','minimal'})do for _,width in ipairs({400,720,1600})do
   for number,track in ipairs({{'Lipstick','Alaia & Gallo'},{'Jóga 🎵 日本語','Björk'},{'Long native title '..string.rep('scrolling ',20),'Long artist '..string.rep('overflow ',12)}})do
    -- Match the actual History provider's normalized metadata. Earlier fixtures
    -- injected trailing spaces directly into GDI and shifted centered text.
    track={track[1]:gsub('%s+$',''),track[2]:gsub('%s+$','')}
    local name=preset..'-'..width..'-'..number
    local settings=native.obs_data_create();native.obs_data_set_string(settings,'preset',preset);native.obs_data_set_int(settings,'width',width)
    native.obs_data_set_string(settings,'vdj_mode','history');native.obs_data_set_string(settings,'path',script_path()..'reference-no-history.txt')
    native.obs_data_set_string(settings,'reference_title',track[1]);native.obs_data_set_string(settings,'reference_artist',track[2])
    local source=native.obs_source_create_private(info.id,name,settings);native.obs_data_release(settings);check(source~=nil,'Original private source')
    native.obs_source_inc_active(source);sources[#sources+1]={source=source,name=name,preset=preset,width=width}
   end
  end end
  stage=1
 elseif stage==1 then
  local item=sources[index];local d=assert(instances[item.name]);local row={}
  for _,key in ipairs({'label','title','artist'})do
   local child=d.children[key];local w,h=obs.obs_source_get_width(child),obs.obs_source_get_height(child)
   if w==0 or h==0 then return end
   row[key]={width=w,height=h};check(w>0 and h>0,'GDI metrics')
  end
  local core=dofile(script_path()..'../../obs-playing/core.lua')
  local layout=core.layout(item.preset,item.width,row,d.line_gap)
  local record={name=item.name,preset=item.preset,title=d.track.title,artist=d.track.artist,width=layout.width,height=layout.height,metrics=row,rows=layout.rows}
  local function json(value)
   if type(value)=='number'then return tostring(value)end
   if type(value)=='string'then return '"'..value:gsub('\\','\\\\'):gsub('"','\\"')..'"'end
   local fields={};for key,v in pairs(value)do fields[#fields+1]='"'..key..'":'..json(v)end;table.sort(fields);return '{'..table.concat(fields,',')..'}'
  end
  metrics[#metrics+1]=json(record)
  probe.playing_source_test_capture_png(item.source,item.name..'.png');requested=requested+1;matte=1;stage=2
 elseif stage==2 and captureCount()>=requested then
  if matte<3 then
   local item=sources[index];probe.playing_source_test_capture_png(item.source,item.name..(matte==1 and '-black.png'or'-white.png'));requested=requested+1;matte=matte+1;return
  end
  index=index+1
  if index<=#sources then stage=1 else
   for _,item in ipairs(sources)do native.obs_source_dec_active(item.source);native.obs_source_release(item.source)end;sources={}
   local file=assert(io.open(script_path()..'normal-reference-metrics.json','wb'));file:write('['..table.concat(metrics,',')..']\n');file:close()
   local result=assert(io.open(script_path()..'normal-reference-result.txt','wb'));result:write('PASS '..checks..' original GDI reference checks; 54 captures; deterministic_t0=1; opaque_mattes=1; native migration pending');result:close()
   stage=3;obs.timer_remove(run)
  end
 end
end
function script_load()obs.timer_add(run,100)end
function script_unload()
 obs.timer_remove(run)
 for _,item in ipairs(sources)do native.obs_source_dec_active(item.source);native.obs_source_release(item.source)end;sources={}
end
