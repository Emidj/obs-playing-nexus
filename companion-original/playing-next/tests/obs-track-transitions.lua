-- Real source/renderer in an isolated OBS. Deterministic pixel samples are
-- diagnostics; separate stages exercise the actual GUI-owned clock and swap.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_update(obs_source_t*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local native=ffi.load('obs.dll');local probe,source,settings
local stage,ticks,checks,index=0,0,0,1;local pending
local presets={0,1,3};local names={'basic','prism','cantilever'}
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function history(title)local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:00 : Artist - '..title..'\r\n');f:close()end
local function read()
 local s={};if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then local raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end;for k,v in raw:gmatch('"([%w]+)":(true)')do s[k]=true end;s.title=raw:match('"title":"([^"]*)"')end end;return s
end
local function configure()
 native.obs_data_set_int(settings,'normal_preset',presets[index]);native.obs_data_set_int(settings,'normal_transition',index-1);native.obs_source_update(source,settings);stage=1
end
local function captures(s)
 pending={count=(s.captures or 0)+6,nextStep=2};stage=-1
 for _,sample in ipairs({'base','full','half','zero','slide','zoom'})do probe.playing_source_test_capture_png(source,names[index]..'-transition-'..sample..'.png')end
end
local function pulse()
 ticks=ticks+1;assert(ticks<230,'Transition timeout stage '..stage)
 local s=read()
 if pending and (s.captures or 0)>=pending.count then stage=pending.nextStep;pending=nil end
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]');check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(module[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  history('Flow A');settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_data_set_int(settings,'normal_duration',2000);native.obs_data_set_bool(settings,'transition_whole',true)
  native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_bool(settings,'artwork_fallback',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'artwork_x',500);native.obs_data_set_int(settings,'artwork_y',10);native.obs_data_set_int(settings,'artwork_width',100);native.obs_data_set_int(settings,'artwork_height',100)
  source=native.obs_source_create_private('obs_playing_next_development','Transitions isolated',settings);check(source~=nil,'Source');native.obs_source_inc_active(source);stage=1
 elseif stage==1 and s.artworks==1 and not s.transitioning then
  check(s.wholeTransition,'Whole presentation flag');check(s.sessions==1 and s.consumers==1,'Shared session');captures(s)
 elseif stage==2 then history('Flow B '..index);stage=3
 elseif stage==3 and s.transitioning and not s.incoming then
  check(s.title==(index==1 and 'Flow A' or 'Flow C '..(index-1)),'OUT replaced title early');check(s.artworks==1,'OUT lost image');history('Flow C '..index);stage=4
 elseif stage==4 and s.incoming and s.title=='Flow C '..index then
  check(s.transitioning,'Midpoint did not enter IN');stage=5
 elseif stage==5 and not s.transitioning and s.title=='Flow C '..index and s.artworks==1 then
  check(s.sessions==1 and s.consumers==1,'Transition restarted reader');pending={count=s.captures+1,nextStep=6};stage=-1;probe.playing_source_test_capture_png(source,names[index]..'-live-final.png')
 elseif stage==6 then
  if index<3 then index=index+1;configure()else history('Hidden pending');stage=7 end
 elseif stage==7 and s.transitioning and not s.incoming then native.obs_source_dec_active(source);stage=8
 elseif stage==8 and s.frames==0 and s.consumers==0 then
  check(not s.animationTimer and s.sessions==0,'Hidden transition retained demand');native.obs_source_inc_active(source);stage=9
 elseif stage==9 and s.title=='Hidden pending' and s.artworks==1 then
  check(not s.transitioning,'Resume replayed stale OUT');native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=10
 elseif stage==10 and s.frames==0 and s.sessions==0 then
  check(not s.animationTimer,'Released transition timer');local f=assert(io.open(script_path()..'track-transitions-result.txt','w'));f:write('PASS checks='..checks..'; paths=BASIC,Prism,Cantilever; transitions=Fade,Slide,Zoom; captures=21; coalesced=1; hidden_resume=1; fixture_only=1');f:close();obslua.timer_remove(pulse);stage=11
 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obslua.timer_add(pulse,100)end
function script_unload()obslua.timer_remove(pulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
