-- Native text-only source, orientation/style/optional-artwork in isolated OBS.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);obs_data_t* obs_data_create_from_json(const char*);void obs_data_release(obs_data_t*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);void obs_data_set_obj(obs_data_t*,const char*,obs_data_t*);int64_t obs_data_get_int(obs_data_t*,const char*);bool obs_data_get_bool(obs_data_t*,const char*);const char* obs_data_get_json(obs_data_t*);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_update(obs_source_t*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);obs_data_t* obs_source_get_settings(const obs_source_t*);
obs_properties_t* obs_source_properties(const obs_source_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);size_t obs_property_list_item_count(obs_property_t*);void obs_properties_destroy(obs_properties_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local native=ffi.load('obs.dll');local probe,source,settings;local stage,ticks,checks=0,0,0;local pending,revision
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function font(key,size)local f=native.obs_data_create();native.obs_data_set_string(f,'face','Arial');native.obs_data_set_int(f,'size',size);native.obs_data_set_int(f,'flags',1);native.obs_data_set_obj(settings,key,f);native.obs_data_release(f)end
local function update(direction,cover)
 native.obs_data_set_int(settings,'text_layout',direction);native.obs_data_set_bool(settings,'artwork_enabled',cover);native.obs_data_set_int(settings,'canvas_width',direction==2 and 320 or 640);native.obs_data_set_int(settings,'canvas_height',direction==2 and 360 or 120);native.obs_source_update(source,settings)
end
local function capture(s,name,nextStage)pending={count=s.captures+1,stage=nextStage};stage=-1;probe.playing_source_test_capture_png(source,'textonly-'..name..'.png')end
local function pulse()
 ticks=ticks+1;assert(ticks<160,'Text-only timeout '..stage);local s={}
 if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then local raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end end
 if pending and (s.captures or 0)>=pending.count then stage=pending.stage;pending=nil end
 if stage==0 then
  local m=ffi.new('obs_module_t*[1]');check(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(m[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:00 : Sonny Fodera - Mind Still (Original Mix)\r\n');f:close()
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_data_set_int(settings,'text_layout',1);native.obs_data_set_int(settings,'text_cover_size',80);native.obs_data_set_int(settings,'title_overflow',3);native.obs_data_set_int(settings,'artist_overflow',3);font('text_title_font',32);font('text_artist_font',20);native.obs_data_set_int(settings,'text_artist_color',0xffffff00);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png')
  source=native.obs_source_create_private('obs_playing_next_development','Text only isolated',settings);check(source~=nil,'Source');local p=native.obs_source_properties(source);check(native.obs_property_list_item_count(native.obs_properties_get(p,'text_layout'))==3,'Orientation options');check(native.obs_properties_get(p,'text_title_font')~=nil and native.obs_properties_get(p,'text_image_gap')~=nil and native.obs_properties_get(p,'artwork_enabled')~=nil,'Style/spacing/cover controls');native.obs_properties_destroy(p);native.obs_source_inc_active(source);stage=1
 elseif stage==1 and s.frames==1 and s.artworks==0 then revision=s.trackRevision;capture(s,'horizontal-off',2)
 elseif stage==2 then update(1,true);stage=3
 elseif stage==3 and s.artworks==1 then check(s.sessions==1 and s.trackRevision==revision,'Cover restarted reader');capture(s,'horizontal-on',4)
 elseif stage==4 then update(2,false);stage=5
 elseif stage==5 and s.artworks==0 and s.frames==1 then capture(s,'vertical-off',6)
 elseif stage==6 then update(2,true);stage=7
 elseif stage==7 and s.artworks==1 then
  local saved=native.obs_source_get_settings(source);local restored=native.obs_data_create_from_json(native.obs_data_get_json(saved));check(native.obs_data_get_int(restored,'text_layout')==2 and native.obs_data_get_bool(restored,'artwork_enabled'),'Scene settings lost orientation/cover');native.obs_data_release(restored);native.obs_data_release(saved);capture(s,'vertical-on',8)
 elseif stage==8 then
  font('text_title_font',28);native.obs_data_set_int(settings,'text_title_color',0xff00ffff);native.obs_data_set_int(settings,'text_margin',24);native.obs_data_set_int(settings,'text_line_gap',24);native.obs_data_set_int(settings,'title_align',2);native.obs_data_set_int(settings,'artist_align',2);update(1,false);stage=9
 elseif stage==9 and s.artworks==0 and s.frames==1 then check(s.trackRevision==revision and s.sessions==1,'Style restarted Core');capture(s,'custom',10)
 elseif stage==10 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=11
 elseif stage==11 and s.frames==0 and s.sessions==0 then local f=assert(io.open(script_path()..'text-only-result.txt','w'));f:write('PASS checks='..checks..'; orientations=2; optional_cover=2; style_spacing=1; scene_roundtrip=1; reader_retained=1; captures=5');f:close();obslua.timer_remove(pulse);stage=12 end
 if probe then probe.playing_source_test_query()end
end
function script_load()obslua.timer_add(pulse,100)end
function script_unload()obslua.timer_remove(pulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
