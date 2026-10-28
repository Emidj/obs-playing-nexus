-- Complete native NORMAL gallery, shared Core, movement/Unicode/scroll/lifecycle.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);obs_data_t* obs_data_create_from_json(const char*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_release(obs_data_t*);const char* obs_data_get_string(obs_data_t*,const char*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_release(obs_source_t*);uint32_t obs_source_get_width(obs_source_t*);uint32_t obs_source_get_height(obs_source_t*);
obs_data_t* obs_source_get_settings(const obs_source_t*);const char* obs_data_get_json(obs_data_t*);int64_t obs_data_get_int(obs_data_t*,const char*);
obs_properties_t* obs_source_properties(const obs_source_t*);void obs_properties_destroy(obs_properties_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);
size_t obs_property_list_item_count(obs_property_t*);int64_t obs_property_list_item_int(obs_property_t*,size_t);const char* obs_property_list_item_name(obs_property_t*,size_t);
void playing_source_test_capture_png(obs_source_t*,const char*);void playing_source_test_query(void);int playing_source_test_read(char*,int);
]]
local native=ffi.load('obs.dll')
local wholeFile=io.open(script_path()..'whole-catalogue.flag','rb');local whole=wholeFile~=nil;if wholeFile then wholeFile:close()end
ffi.cdef[[void obs_data_set_bool(obs_data_t*,const char*,bool);]]
local ids={'studio','minimal','cantilever','oblique','aura','orbit','terminal','rupture','dune','chrome','monolith','portal','beacon','laser','matrix','lotus','tide','silk','obsidian','lens','gum','sintonia','trazo','neon','broadcast','editorial'}
local names={'Prism','Air','Cantilever','Oblique','Aura','Orbit','Terminal','Rupture','Dune','Chrome','Monolith','Portal','Beacon','Laser','Matrix','Lotus','Tide','Silk','Obsidian','Lens','Bubblegum','Tuning','Trace','Neon (Orbit)','Broadcast (Oblique)','Editorial (Chrome)'}
local probe,sources=nil,{}
local stage,ticks,checks,captures,waitTick=0,0,0,0,0
local unicode='Jóga 🎵 日本語'
local long=('Long native title '..string.rep('scrolling ',45)):gsub('%s+$','')
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function history(title,artist)local file=assert(io.open(script_path()..'catalogue-history.txt','wb'));file:write('21:05 : '..artist..' - '..title..'\r\n');file:close()end
local function read()
 local buffer=ffi.new('char[4096]');local n=probe.playing_source_test_read(buffer,4096);local text=n>0 and ffi.string(buffer,n)or'';local s={}
 for key,value in text:gmatch('"([%w]+)":(%d+)')do s[key]=tonumber(value)end
 s.animation=text:match('"animationTimer":true')~=nil;s.timer=text:match('"timer":true')~=nil
 if n>0 then local data=native.obs_data_create_from_json(text);s.title=ffi.string(native.obs_data_get_string(data,'title'));native.obs_data_release(data)end
 return s
end
local function gallery(suffix)
 for i,source in ipairs(sources)do
  check(native.obs_source_get_width(source)==720 and native.obs_source_get_height(source)>100 and native.obs_source_get_height(source)<=2048,'Source dimensions '..ids[i])
  probe.playing_source_test_capture_png(source,ids[i]..'-'..suffix..'.png');captures=captures+1
 end
end
local function pulse()
 ticks=ticks+1;check(ticks<200,'NORMAL catalogue timeout at '..stage)
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open');check(native.obs_init_module(module[0]),'Module init');probe=ffi.load(script_path()..'obs-playing-next.dll');history('Lipstick','Alaia & Gallo')
  for i,id in ipairs(ids)do
   local settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'catalogue-history.txt');native.obs_data_set_int(settings,'normal_preset',i);native.obs_data_set_int(settings,'canvas_width',720);native.obs_data_set_int(settings,'normal_transition',i%3);native.obs_data_set_int(settings,'normal_duration',200)
   if whole then native.obs_data_set_bool(settings,'transition_whole',true)end
   local source=native.obs_source_create_private('obs_playing_next_development',id,settings);native.obs_data_release(settings);check(source~=nil,'NORMAL source '..id);native.obs_source_inc_active(source);sources[#sources+1]=source
  end
  local properties=native.obs_source_properties(sources[1]);check(properties~=nil,'Native properties missing');local selector=native.obs_properties_get(properties,'normal_preset');check(selector~=nil and native.obs_property_list_item_count(selector)==27,'Saved selector count changed');
  check(native.obs_property_list_item_int(selector,0)==0,'BASIC setting changed');
  for i,source in ipairs(sources)do
   check(native.obs_property_list_item_int(selector,i)==i and ffi.string(native.obs_property_list_item_name(selector,i))==names[i],'Saved selector mapping '..ids[i]);
   local saved=native.obs_source_get_settings(source);local restored=native.obs_data_create_from_json(native.obs_data_get_json(saved));check(restored~=nil and native.obs_data_get_int(restored,'normal_preset')==i,'Saved settings roundtrip '..ids[i]);native.obs_data_release(restored);native.obs_data_release(saved)
  end
  native.obs_properties_destroy(properties)
  stage=1
 else
  local s=read()
  if stage==1 and s.frames==26 then
   check(s.sessions==1 and s.consumers==26 and s.animation,'Catalogue did not share demand');gallery('short');if whole then gallery('transition-base');gallery('transition-full');gallery('transition-half')end;stage=2
  elseif stage==2 and s.captures==captures then
   check(s.capturePixels>0,'Gallery GPU empty');waitTick=ticks;stage=3
  elseif stage==3 and ticks-waitTick>=12 then gallery('motion');stage=4
  elseif stage==4 and s.captures==captures then history(unicode,'Björk');waitTick=ticks;stage=5
  elseif stage==5 and ticks-waitTick>=8 and s.title==unicode then gallery('unicode');stage=6
  elseif stage==6 and s.captures==captures then history(long,string.rep('Long artist ',25));waitTick=ticks;stage=7
  elseif stage==7 and ticks-waitTick>=8 and s.title==long then gallery('long');stage=8
  elseif stage==8 and s.captures==captures then
   gallery('text-a');stage=81
  elseif stage==81 and s.captures==captures then waitTick=ticks;stage=82
  elseif stage==82 and ticks-waitTick>=25 then gallery('text-b');stage=83
  elseif stage==83 and s.captures==captures then
   for _,source in ipairs(sources)do native.obs_source_dec_active(source)end;waitTick=ticks;stage=9
  elseif stage==9 and s.consumers==0 and ticks-waitTick>=4 then
   check(s.frames==0 and s.sessions==0 and not s.timer and not s.animation and s.uploads==0,'Hidden catalogue retains work');for _,source in ipairs(sources)do native.obs_source_inc_active(source)end;waitTick=ticks;stage=10
  elseif stage==10 and s.frames==26 and s.title==long and ticks-waitTick>=4 then
   check(s.sessions==1 and s.consumers==26 and s.animation,'Resume demand');probe.playing_source_test_capture_png(sources[1],'studio-resume.png');probe.playing_source_test_capture_png(sources[7],'terminal-resume.png');captures=captures+2;stage=11
  elseif stage==11 and s.captures==captures then
   for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={};waitTick=ticks;stage=12
  elseif stage==12 and s.frames==0 and s.sessions==0 and ticks-waitTick>=3 then
   check(captures==(whole and 236 or 158) and not s.animation and not s.timer and s.uploads==0,'Final catalogue lifecycle');local file=assert(io.open(script_path()..'normal-catalogue-result.txt','wb'));file:write('PASS '..checks..' complete native NORMAL checks; presets=23; aliases=3; captures='..captures..'; shared_session=1; transitions=3; unicode=1; text_planes=1; hide_resume=1; inactive_animation=0; whole='..tostring(whole));file:close();obs.timer_remove(pulse);stage=13
  end
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obs.timer_add(pulse,100)end
function script_unload()obs.timer_remove(pulse);for _,source in ipairs(sources)do native.obs_source_dec_active(source);native.obs_source_release(source)end;sources={}end
