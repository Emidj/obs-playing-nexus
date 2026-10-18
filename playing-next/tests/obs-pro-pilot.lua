-- Actual PRO source/renderer, isolated OBS, local history and profile fixtures.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t;typedef struct obs_source obs_source_t;typedef struct obs_data obs_data_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
void* obs_obj_get_data(void*);bool obs_property_button_clicked(obs_property_t*,void*);
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
obs_data_t* obs_data_create(void);void obs_data_release(obs_data_t*);void obs_data_set_string(obs_data_t*,const char*,const char*);void obs_data_set_int(obs_data_t*,const char*,int64_t);void obs_data_set_bool(obs_data_t*,const char*,bool);
obs_source_t* obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_update(obs_source_t*,obs_data_t*);void obs_source_release(obs_source_t*);void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);
obs_properties_t* obs_source_properties(const obs_source_t*);obs_property_t* obs_properties_get(obs_properties_t*,const char*);size_t obs_property_list_item_count(obs_property_t*);int64_t obs_property_list_item_int(obs_property_t*,size_t);void obs_properties_destroy(obs_properties_t*);
bool obs_property_modified(obs_property_t*,obs_data_t*);int64_t obs_data_get_int(obs_data_t*,const char*);bool obs_property_visible(obs_property_t*);const char* obs_property_list_item_string(obs_property_t*,size_t);obs_property_t* obs_properties_first(obs_properties_t*);bool obs_property_next(obs_property_t**);const char* obs_property_name(obs_property_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture_png(obs_source_t*,const char*);
]]
local verticalFile=io.open(script_path()..'vertical-geometry-only.flag','r');local verticalOnly=verticalFile~=nil;if verticalFile then verticalFile:close()end
local native=ffi.load('obs.dll');local probe,source,settings;local stage,ticks,checks=0,0,0;local pending,revision,rasters,uploads,ready,vinylBaseX,vinylBaseY,urbanBaseX,urbanBaseY,urbanArtistY
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function update(key,value)native.obs_data_set_int(settings,key,value);native.obs_source_update(source,settings);ready=ticks+3 end
-- Lua concatenation is explicit; no source-control/product automation hooks.
local function shot(s,name,nextStage)
 local f=assert(io.open(script_path()..'pro-'..name..'-bounds.json','w'));f:write(string.format('{"titleX":%d,"titleY":%d,"titleWidth":%d,"titleHeight":%d,"artistX":%d,"artistY":%d,"artistWidth":%d,"artistHeight":%d}',s.titleX,s.titleY,s.titleWidth,s.titleHeight,s.artistX,s.artistY,s.artistWidth,s.artistHeight));f:close()
 pending={count=s.captures+1,stage=nextStage};stage=-1;probe.playing_source_test_capture_png(source,'pro-'..name..'.png')
end
local function pulse()
 ticks=ticks+1;assert(ticks<1150,'PRO timeout '..stage);local s={}
 if probe then local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);if n>0 then local raw=ffi.string(b,n);for k,v in raw:gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end;s.animation=raw:find('"animationTimer":true',1,true)~=nil;s.retains=raw:find('"appearanceRetained":true',1,true)~=nil;s.demo=raw:find('"demoTrack":true',1,true)~=nil end end
 if pending and (s.captures or 0)>=pending.count then stage=pending.stage;pending=nil end
 if stage==0 then
  local m=ffi.new('obs_module_t*[1]');check(native.obs_open_module(m,script_path()..'obs-playing-next.dll',script_path())==0,'Open');check(native.obs_init_module(m[0]),'Init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:00 : Sonny Fodera - Mind Still (Original Mix)\r\n');f:close()
  settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_data_set_int(settings,'pro_preset',1);native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',200);native.obs_data_set_int(settings,'text_margin',32);native.obs_data_set_int(settings,'text_cover_size',96);native.obs_data_set_int(settings,'text_artist_color',0xffb400ff);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'pro_light_intensity',0)
  source=native.obs_source_create_private('obs_playing_next_development','PRO pilot isolated',settings);check(source~=nil,'Source');local p=native.obs_source_properties(source);local selector=native.obs_properties_get(p,'pro_preset');check(native.obs_property_list_item_count(selector)==17,'Pilot choices');check(native.obs_properties_get(p,'performance_profile')==nil,'Removed performance selector remains');check(native.obs_properties_get(p,'pro_hue')~=nil and native.obs_properties_get(p,'pro_light_intensity')~=nil,'PRO sliders');check(native.obs_properties_get(p,'layer_animation')==nil,'Removed movement selector remains');
  local ui=native.obs_data_create();native.obs_data_set_int(ui,'pro_preset',1);native.obs_property_modified(selector,ui);check(native.obs_data_get_int(ui,'canvas_width')==960 and native.obs_data_get_int(ui,'canvas_height')==367,'PRO UI default dimensions');native.obs_data_set_int(ui,'canvas_width',800);native.obs_property_modified(selector,ui);check(native.obs_data_get_int(ui,'canvas_width')==800,'PRO UI overwrote custom dimensions');local dj=native.obs_properties_get(p,'provider');check(native.obs_properties_first(p)==dj,'DJ selector is not first');check(native.obs_properties_get(p,'streamer_image')==nil and native.obs_properties_get(p,'choose_streamer_image')~=nil,'Manual image must be a file-dialog button');native.obs_property_button_clicked(native.obs_properties_get(p,'reload_artwork'),native.obs_obj_get_data(source));local order={};local cursor=ffi.new('obs_property_t*[1]',native.obs_properties_first(p));repeat order[#order+1]=ffi.string(native.obs_property_name(cursor[0])) until not native.obs_property_next(cursor);check(order[1]=='provider' and order[2]=='mode' and order[3]=='preset_family' and order[4]=='preset_style','DJ/reading/category/style order');check(order[#order]=='nexus_credit' and order[#order-1]=='demo_image' and order[#order-2]=='demo_next' and order[#order-3]=='demo_previous' and order[#order-4]=='demo_enabled','Test controls are not last');check(native.obs_properties_get(p,'demo_enabled')~=nil and native.obs_properties_get(p,'animation_speed')~=nil,'Demo/speed controls missing');local rekordbox=false;for i=0,tonumber(native.obs_property_list_item_count(dj))-1 do rekordbox=rekordbox or ffi.string(native.obs_property_list_item_string(dj,i))=='rekordbox' end;check(rekordbox,'Rekordbox missing from DJ list');native.obs_data_set_bool(ui,'demo_enabled',true);native.obs_property_modified(native.obs_properties_get(p,'demo_enabled'),ui);check(native.obs_property_visible(native.obs_properties_get(p,'mode')),'Reading selector hidden during demo');check(native.obs_properties_get(p,'demo_title')==nil and native.obs_properties_get(p,'demo_artist')==nil,'Unnecessary demo text fields');local category=native.obs_properties_get(p,'preset_family');local style=native.obs_properties_get(p,'preset_style');check(category~=nil and style~=nil,'Category/style controls absent');native.obs_data_set_int(ui,'preset_family',0);native.obs_property_modified(category,ui);check(native.obs_data_get_int(ui,'pro_preset')==0 and native.obs_data_get_int(ui,'normal_preset')~=0 and native.obs_property_list_item_count(style)==24,'Classic style list/exclusion');native.obs_data_set_int(ui,'preset_family',1);native.obs_property_modified(category,ui);check(native.obs_data_get_int(ui,'normal_preset')==0 and native.obs_property_list_item_count(style)==17,'PRO style list/exclusion');native.obs_data_set_int(ui,'preset_style',3);native.obs_property_modified(style,ui);check(native.obs_data_get_int(ui,'pro_preset')==3 and not native.obs_property_visible(selector),'Style selection/hidden legacy selector');-- Geometry must reset on every style change, including return to a previous style.
-- The category selector must also restore every classic style's geometry.
native.obs_data_set_int(ui,'preset_family',0);native.obs_property_modified(category,ui)
local classicCount=tonumber(native.obs_property_list_item_count(style))
for pass=1,2 do for item=0,classicCount-1 do
 native.obs_data_set_int(ui,'canvas_width',1024);native.obs_data_set_int(ui,'canvas_height',900);native.obs_data_set_int(ui,'text_offset_x',57);native.obs_data_set_int(ui,'text_offset_y',-33)
 native.obs_data_set_int(ui,'preset_style',native.obs_property_list_item_int(style,item));native.obs_property_modified(style,ui)
 check(native.obs_data_get_int(ui,'canvas_width')==640 and native.obs_data_get_int(ui,'canvas_height')==120,'Classic retained previous canvas dimensions')
 check(native.obs_data_get_int(ui,'text_offset_x')==0 and native.obs_data_get_int(ui,'text_offset_y')==0,'Classic retained previous offsets')
end end
native.obs_data_set_int(ui,'preset_family',1);native.obs_property_modified(category,ui)

for _,choice in ipairs({14,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}) do
 native.obs_data_set_int(ui,'canvas_width',400);native.obs_data_set_int(ui,'canvas_height',720);native.obs_data_set_int(ui,'text_top_distance',445);native.obs_data_set_int(ui,'text_cover_size',32);native.obs_data_set_int(ui,'text_offset_x',57);native.obs_data_set_int(ui,'text_offset_y',-33)
 native.obs_data_set_int(ui,'preset_style',choice);native.obs_property_modified(style,ui)
 check(native.obs_data_get_int(ui,'canvas_width')==(choice==2 and 640 or choice==3 and 320 or choice>=12 and (choice==12 and 480 or choice==13 and 416 or choice==14 and 400 or choice==15 and 384 or 480) or 960),'Style retained previous canvas width')
 check(native.obs_data_get_int(ui,'canvas_height')==(choice==2 and 120 or choice==3 and 360 or choice>=12 and 720 or choice>=7 and 320 or choice>=5 and 280 or 367),'Style retained previous canvas height')
 check(native.obs_data_get_int(ui,'text_top_distance')~=445,'Style retained previous text offset');check(native.obs_data_get_int(ui,'text_offset_x')==0 and native.obs_data_get_int(ui,'text_offset_y')==0,'Style retained signed text offsets')
end
native.obs_data_set_int(ui,'editor_mode',2);native.obs_property_modified(native.obs_properties_get(p,'editor_mode'),ui);check(native.obs_property_visible(native.obs_properties_get(p,'text_offset_x')) and native.obs_property_visible(native.obs_properties_get(p,'text_offset_y')),'Text sliders missing');for _,old in ipairs({'text_margin','text_line_gap','text_top_distance','text_image_gap','text_cover_size'})do check(not native.obs_property_visible(native.obs_properties_get(p,old)),'Obsolete geometry control visible')end;native.obs_data_release(ui);native.obs_properties_destroy(p);native.obs_source_inc_active(source);stage=1;if verticalOnly then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=353 end
 elseif stage==1 and s.artworks==1 then revision=s.trackRevision;check(not s.animation,'Static PRO timer');shot(s,'off',2)
 elseif stage==2 then rasters=s.renders;update('pro_light_intensity',100);stage=3
 elseif stage==3 and ticks>=ready and s.artworks==1 then check(s.renders==rasters and not s.animation,'Intensity rebuilt graphics');shot(s,'max',4)
 elseif stage==4 then rasters=s.renders;uploads=s.uploads;update('pro_hue',120);stage=5
 elseif stage==5 and ticks>=ready and s.artworks==1 then check(s.sessions==1 and s.trackRevision==revision,'Hue restarted Core');check(s.retains and not s.animation,'Hue blanked rendering/restarted transition');check(s.renders==rasters and s.uploads==uploads,'GPU hue rebuilt/uploaded textures');shot(s,'hue120',6)
 elseif stage==6 then update('pro_hue',360);stage=7
 elseif stage==7 and ticks>=ready and s.artworks==1 then shot(s,'original',8)
 elseif stage==8 then rasters=s.renders;uploads=s.uploads;native.obs_data_set_bool(settings,'pro_light_pulse',true);native.obs_source_update(source,settings);ready=ticks+8;stage=9
 elseif stage==9 and ticks>=ready then check(s.animation and s.renders==rasters and s.uploads==uploads,'Pulse rerasterized/uploaded');update('pro_light_intensity',0);stage=10
 elseif stage==10 and ticks>=ready then check(not s.animation,'Zero pulse timer');native.obs_data_set_bool(settings,'pro_light_pulse',false);native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+3;stage=11
 elseif stage==11 and ticks>=ready and s.artworks==0 then shot(s,'no-cover',12)
 elseif stage==12 then update('pro_panel_opacity',0);stage=122
 elseif stage==122 and ticks>=ready then rasters=s.renders;shot(s,'panel-zero',123)
 elseif stage==123 then update('pro_panel_opacity',100);stage=124
 elseif stage==124 and ticks>=ready then check(s.renders==rasters,'Panel opacity rerasterized');shot(s,'panel-full',125)
 elseif stage==125 then local props=native.obs_source_properties(source);native.obs_property_button_clicked(native.obs_properties_get(props,'demo_next'),source);native.obs_properties_destroy(props);ready=ticks+2;stage=126
 elseif stage==126 and ticks>=ready then shot(s,'text-transition',127)
 elseif stage==127 then update('normal_transition',1);stage=128
 elseif stage==128 and ticks>=ready then local props=native.obs_source_properties(source);native.obs_property_button_clicked(native.obs_properties_get(props,'demo_next'),source);native.obs_properties_destroy(props);ready=ticks+2;stage=129
 elseif stage==129 and ticks>=ready then shot(s,'slide-transition',130)
 elseif stage==130 then update('normal_transition',2);stage=131
 elseif stage==131 and ticks>=ready then local props=native.obs_source_properties(source);native.obs_property_button_clicked(native.obs_properties_get(props,'demo_previous'),source);native.obs_properties_destroy(props);ready=ticks+2;stage=132
 elseif stage==132 and ticks>=ready then shot(s,'zoom-transition',133)
 elseif stage==133 then update('pro_preset',666);stage=121
 elseif stage==121 and ticks>=ready then check(s.proArtReady==0,'Unknown PRO setting became Neon Club');native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=13
 elseif stage==13 and s.frames==0 and s.sessions==0 then
  check(not s.animation,'Hidden timer');settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_string(settings,'demo_title','A very long title for real ping pong movement in the native renderer');native.obs_data_set_string(settings,'demo_artist','Fictitious artist');native.obs_data_set_int(settings,'pro_preset',2);native.obs_data_set_int(settings,'canvas_width',320);native.obs_data_set_int(settings,'canvas_height',120);native.obs_data_set_int(settings,'title_overflow',4);native.obs_data_set_int(settings,'normal_speed',150);native.obs_data_set_int(settings,'normal_pause',0);
  local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write('21:01 : Artist - A very long title for real ping pong movement in the native renderer\r\n');f:close();source=native.obs_source_create_private('obs_playing_next_development','Ping pong isolated',settings);native.obs_source_inc_active(source);ready=ticks+4;stage=14
 elseif stage==14 and ticks>=ready and s.frames==1 then check(s.animation,'Overflow ping pong timer absent');check(s.sessions==0,'Fictitious preview acquired DJ provider');rasters=s.renders;shot(s,'ping-start',15)
 elseif stage==15 then ready=ticks+10;stage=16
 elseif stage==16 and ticks>=ready then check(s.animation and s.renders==rasters,'Ping pong rerasterized');shot(s,'ping-moved',17)
 elseif stage==17 then native.obs_data_set_int(settings,'pro_preset',0);native.obs_data_set_int(settings,'normal_preset',1);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'streamer_image',script_path()..'fixture-red.png');native.obs_source_update(source,settings);ready=ticks+4;stage=18
 elseif stage==18 and ticks>=ready then check(s.artworks==0,'Classic unexpectedly has artwork');native.obs_data_set_bool(settings,'demo_enabled',false);native.obs_data_set_int(settings,'mode',1);native.obs_data_set_string(settings,'history',script_path()..'source-history.txt');native.obs_source_update(source,settings);ready=ticks+5;stage=181
 elseif stage==181 and ticks>=ready and s.sessions==1 and s.frames==1 then check(not s.demo,'Demo toggle did not restore real provider');local props=native.obs_source_properties(source);local button=native.obs_properties_get(props,'demo_next');check(button~=nil,'Next track button missing');check(native.obs_property_button_clicked(button,source),'Preview button rejected');native.obs_properties_destroy(props);ready=ticks+5;stage=182
 elseif stage==182 and ticks>=ready and s.sessions==0 and s.frames==1 then check(s.demo,'Preview button did not publish fictitious track');native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=19
 elseif stage==19 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',4);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',367);native.obs_data_set_int(settings,'text_cover_size',216);source=native.obs_source_create_private('obs_playing_next_development','Vinyl isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=191
 elseif stage==191 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0,'Vinyl animation or demo demand missing');check(s.artworkWidth>0 and s.artworkHeight>0,'Vinyl cover not square');check(s.recordBottom<=315,'Record exceeds lower frame');rasters=s.renders;shot(s,'vinyl-start',192)
 elseif stage==192 then ready=ticks+12;stage=193
 elseif stage==193 and ticks>=ready then check(s.renders==rasters,'Record rotation rerasterized');shot(s,'vinyl-moved',194)
 elseif stage==194 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_data_set_int(settings,'pro_light_intensity',0);native.obs_source_update(source,settings);ready=ticks+4;stage=196
 elseif stage==196 and ticks>=ready and s.artworks==0 then shot(s,'vinyl-half',197)
 elseif stage==197 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=195
 elseif stage==195 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',5);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',280);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Minimal isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=201
 elseif stage==201 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Minimal demand or square cover');rasters=s.renders;shot(s,'minimal-start',202)
 elseif stage==202 then ready=ticks+15;stage=203
 elseif stage==203 and ticks>=ready then check(s.renders==rasters,'Minimal sheen rerasterized');shot(s,'minimal-moved',204)
 elseif stage==204 then update('pro_hue',120);stage=205
 elseif stage==205 and ticks>=ready then check(s.renders==rasters,'Minimal hue rerasterized');shot(s,'minimal-hue',206)
 elseif stage==206 then update('pro_light_intensity',0);stage=207
 elseif stage==207 and ticks>=ready then check(not s.animation,'Minimal zero light retained timer');shot(s,'minimal-off',208)
 elseif stage==208 then update('pro_panel_opacity',0);stage=209
 elseif stage==209 and ticks>=ready then shot(s,'minimal-transparent',210)
 elseif stage==210 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=211
 elseif stage==211 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',6);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',280);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Glassmorph isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=221
 elseif stage==221 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Glass demand or square cover');rasters=s.renders;shot(s,'glass-start',222)
 elseif stage==222 then ready=ticks+15;stage=223
 elseif stage==223 and ticks>=ready then check(s.renders==rasters,'Glass reflection rerasterized');shot(s,'glass-moved',224)
 elseif stage==224 then update('pro_light_intensity',0);stage=225
 elseif stage==225 and ticks>=ready then check(not s.animation,'Glass zero light retained timer');shot(s,'glass-off',226)
 elseif stage==226 then update('pro_hue',120);stage=227
 elseif stage==227 and ticks>=ready then check(s.renders==rasters,'Glass hue rerasterized');shot(s,'glass-hue',228)
 elseif stage==228 then update('pro_panel_opacity',0);stage=229
 elseif stage==229 and ticks>=ready then shot(s,'glass-transparent',230)
 elseif stage==230 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=231
 elseif stage==231 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',7);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',320);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Urban isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=241
 elseif stage==241 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Urban demand or square cover');rasters=s.renders;shot(s,'urban-start',242)
 elseif stage==242 then ready=ticks+15;stage=243
 elseif stage==243 and ticks>=ready then check(s.renders==rasters,'Urban paint animation rerasterized');shot(s,'urban-moved',244)
 elseif stage==244 then update('pro_light_intensity',0);stage=245
 elseif stage==245 and ticks>=ready then check(not s.animation,'Urban zero light retained timer');shot(s,'urban-off',246)
 elseif stage==246 then update('pro_hue',120);stage=247
 elseif stage==247 and ticks>=ready then check(s.renders==rasters,'Urban hue rerasterized');shot(s,'urban-hue',248)
 elseif stage==248 then update('pro_panel_opacity',0);stage=249
 elseif stage==249 and ticks>=ready then shot(s,'urban-transparent',250)
 elseif stage==250 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=251
 elseif stage==251 and ticks>=ready and s.artworks==0 then shot(s,'urban-no-cover',252)
 elseif stage==252 then urbanBaseX=s.titleX;urbanBaseY=s.titleY;urbanArtistY=s.artistY;update('text_offset_y',-10);stage=261
 elseif stage==261 and ticks>=ready then check(s.titleY==urbanBaseY-10 and s.artistY==urbanArtistY-10,'Vertical slider does not move both text lines');shot(s,'urban-gap-base',262)
 elseif stage==262 then update('text_offset_x',-10);stage=263
 elseif stage==263 and ticks>=ready then check(s.titleX==urbanBaseX-10 and s.titleY==urbanBaseY-10 and s.retains,'Horizontal slider moved wrong axis or blanked appearance');shot(s,'urban-gap-wide',264)
 elseif stage==264 then update('text_offset_y',10);stage=265
 elseif stage==265 and ticks>=ready then check(s.titleY==urbanBaseY+10 and s.artistY==urbanArtistY+10 and s.retains,'Positive slider does not move both lines or retain appearance');shot(s,'urban-top-distance',266)
 elseif stage==266 then update('text_offset_y',0);stage=267
 elseif stage==267 and ticks>=ready then check(s.titleY==urbanBaseY and s.artistY==urbanArtistY,'Zero vertical slider did not restore original');shot(s,'urban-touching',268)
 elseif stage==268 then update('text_offset_x',0);stage=269
 elseif stage==269 and ticks>=ready then check(s.titleX==urbanBaseX and s.retains,'Zero horizontal slider did not restore original');shot(s,'urban-overlap',270)
 elseif stage==270 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=253
 elseif stage==253 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',8);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',320);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Waveform isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=281
 elseif stage==281 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Wave demand or square cover');rasters=s.renders;shot(s,'wave-start',282)
 elseif stage==282 then ready=ticks+15;stage=283
 elseif stage==283 and ticks>=ready then check(s.renders==rasters,'Wave reflections rerasterized');shot(s,'wave-moved',284)
 elseif stage==284 then update('pro_light_intensity',0);stage=285
 elseif stage==285 and ticks>=ready then check(not s.animation,'Wave zero light retained timer');shot(s,'wave-off',286)
 elseif stage==286 then update('pro_hue',120);stage=287
 elseif stage==287 and ticks>=ready then check(s.renders==rasters,'Wave hue rerasterized');shot(s,'wave-hue',288)
 elseif stage==288 then update('pro_panel_opacity',0);stage=289
 elseif stage==289 and ticks>=ready then shot(s,'wave-transparent',290)
 elseif stage==290 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=291
 elseif stage==291 and ticks>=ready and s.artworks==0 then shot(s,'wave-no-cover',292)
 elseif stage==292 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=293
 elseif stage==293 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',9);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',320);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','HUD isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=301
 elseif stage==301 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'HUD demand or square cover');rasters=s.renders;shot(s,'hud-start',302)
 elseif stage==302 then ready=ticks+15;stage=303
 elseif stage==303 and ticks>=ready then check(s.renders==rasters,'HUD scan rerasterized');shot(s,'hud-moved',304)
 elseif stage==304 then update('pro_light_intensity',0);stage=305
 elseif stage==305 and ticks>=ready then check(not s.animation,'HUD zero light retained timer');shot(s,'hud-off',306)
 elseif stage==306 then update('pro_hue',120);stage=307
 elseif stage==307 and ticks>=ready then check(s.renders==rasters,'HUD hue rerasterized');shot(s,'hud-hue',308)
 elseif stage==308 then update('pro_panel_opacity',0);stage=309
 elseif stage==309 and ticks>=ready then shot(s,'hud-transparent',310)
 elseif stage==310 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=311
 elseif stage==311 and ticks>=ready and s.artworks==0 then shot(s,'hud-no-cover',312)
 elseif stage==312 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=313
 elseif stage==313 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',10);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',320);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Particles isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=321
 elseif stage==321 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Particles demand or square cover');rasters=s.renders;shot(s,'particles-start',322)
 elseif stage==322 then ready=ticks+15;stage=323
 elseif stage==323 and ticks>=ready then check(s.renders==rasters,'Moving prisms rerasterized');shot(s,'particles-moved',324)
 elseif stage==324 then update('pro_light_intensity',0);stage=325
 elseif stage==325 and ticks>=ready then check(s.animation,'Particles light zero stopped prism movement');shot(s,'particles-off',326)
 elseif stage==326 then update('pro_hue',120);stage=327
 elseif stage==327 and ticks>=ready then check(s.renders==rasters,'Particles hue rerasterized');shot(s,'particles-hue',328)
 elseif stage==328 then update('pro_panel_opacity',0);stage=329
 elseif stage==329 and ticks>=ready then shot(s,'particles-transparent',330)
 elseif stage==330 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=331
 elseif stage==331 and ticks>=ready and s.artworks==0 then shot(s,'particles-no-cover',332)
 elseif stage==332 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=333
 elseif stage==333 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',11);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',960);native.obs_data_set_int(settings,'canvas_height',320);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Liquid isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=341
 elseif stage==341 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Liquid demand or circular cover bounds');rasters=s.renders;shot(s,'liquid-start',342)
 elseif stage==342 then ready=ticks+15;stage=343
 elseif stage==343 and ticks>=ready then check(s.renders==rasters,'Liquid reflections rerasterized');shot(s,'liquid-moved',344)
 elseif stage==344 then update('pro_light_intensity',0);stage=345
 elseif stage==345 and ticks>=ready then check(not s.animation,'Liquid zero light retained timer');shot(s,'liquid-off',346)
 elseif stage==346 then update('pro_hue',120);stage=347
 elseif stage==347 and ticks>=ready then check(s.renders==rasters,'Liquid hue rerasterized');shot(s,'liquid-hue',348)
 elseif stage==348 then update('pro_panel_opacity',0);stage=349
 elseif stage==349 and ticks>=ready then shot(s,'liquid-transparent',350)
 elseif stage==350 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=351
 elseif stage==351 and ticks>=ready and s.artworks==0 then shot(s,'liquid-no-cover',352)
 elseif stage==352 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=353
 elseif stage==353 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',12);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',480);native.obs_data_set_int(settings,'canvas_height',720);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Retro isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=361
 elseif stage==361 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Retro demand or square cover bounds');rasters=s.renders;shot(s,'retro-start',362)
 elseif stage==362 then ready=ticks+15;stage=363
 elseif stage==363 and ticks>=ready then check(s.renders==rasters,'Retro reflections rerasterized');shot(s,'retro-moved',364)
 elseif stage==364 then update('pro_light_intensity',0);stage=365
 elseif stage==365 and ticks>=ready then check(not s.animation,'Retro zero light retained timer');shot(s,'retro-off',366)
 elseif stage==366 then update('pro_hue',120);stage=367
 elseif stage==367 and ticks>=ready then check(s.renders==rasters,'Retro hue rerasterized');shot(s,'retro-hue',368)
 elseif stage==368 then update('pro_panel_opacity',50);stage=374
 elseif stage==374 and ticks>=ready then shot(s,'retro-half',375)
 elseif stage==375 then update('pro_panel_opacity',0);stage=369
 elseif stage==369 and ticks>=ready then shot(s,'retro-transparent',370)
 elseif stage==370 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=371
 elseif stage==371 and ticks>=ready and s.artworks==0 then shot(s,'retro-no-cover',372)
 elseif stage==372 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=373
 elseif stage==373 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',13);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',416);native.obs_data_set_int(settings,'canvas_height',720);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Vertical Glass isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=381
 elseif stage==381 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Vertical Glass demand or square cover bounds');rasters=s.renders;shot(s,'vglass-start',382)
 elseif stage==382 then ready=ticks+15;stage=383
 elseif stage==383 and ticks>=ready then check(s.renders==rasters,'Vertical Glass reflections rerasterized');shot(s,'vglass-moved',384)
 elseif stage==384 then update('pro_light_intensity',0);stage=385
 elseif stage==385 and ticks>=ready then check(not s.animation,'Vertical Glass zero light retained timer');shot(s,'vglass-off',386)
 elseif stage==386 then update('pro_hue',120);stage=387
 elseif stage==387 and ticks>=ready then check(s.renders==rasters,'Vertical Glass hue rerasterized');shot(s,'vglass-hue',388)
 elseif stage==388 then update('pro_panel_opacity',50);stage=394
 elseif stage==394 and ticks>=ready then shot(s,'vglass-half',395)
 elseif stage==395 then update('pro_panel_opacity',0);stage=389
 elseif stage==389 and ticks>=ready then shot(s,'vglass-transparent',390)
 elseif stage==390 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=391
 elseif stage==391 and ticks>=ready and s.artworks==0 then shot(s,'vglass-no-cover',392)
 elseif stage==392 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=393
 elseif stage==393 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',14);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',400);native.obs_data_set_int(settings,'canvas_height',720);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Vertical Vinyl isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=401
 elseif stage==401 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Vertical Vinyl demand or square cover bounds');check(math.abs(s.titleX+s.titleWidth/2-200)<=.5 and math.abs(s.artistX+s.artistWidth/2-200)<=.5,'Vertical Vinyl text boxes are not centred in the frame');rasters=s.renders;vinylBaseX=s.titleX;vinylBaseY=s.titleY;shot(s,'vvinyl-start',402)
 elseif stage==402 then native.obs_data_set_int(settings,'text_offset_x',20);native.obs_data_set_int(settings,'text_offset_y',-20);native.obs_source_update(source,settings);ready=ticks+15;stage=403
 elseif stage==403 and ticks>=ready then check(s.renders==rasters,'Vertical Vinyl reflections/offsets rerasterized');check(s.titleX==vinylBaseX+20 and s.titleY==vinylBaseY-20 and s.retains,'Text sliders failed or blanked visible frame');shot(s,'vvinyl-moved',404)
 elseif stage==404 then native.obs_data_set_int(settings,'text_offset_x',0);native.obs_data_set_int(settings,'text_offset_y',0);update('pro_light_intensity',0);stage=405
 elseif stage==405 and ticks>=ready then check(s.animation,'Vertical Vinyl light zero stopped rotation');check(s.titleX==vinylBaseX and s.titleY==vinylBaseY,'Zero sliders failed to restore original text');shot(s,'vvinyl-off',406)
 elseif stage==406 then update('pro_hue',120);stage=407
 elseif stage==407 and ticks>=ready then check(s.renders==rasters,'Vertical Vinyl hue rerasterized');shot(s,'vvinyl-hue',408)
 elseif stage==408 then update('pro_panel_opacity',50);stage=414
 elseif stage==414 and ticks>=ready then shot(s,'vvinyl-half',415)
 elseif stage==415 then update('pro_panel_opacity',0);stage=409
 elseif stage==409 and ticks>=ready then shot(s,'vvinyl-transparent',410)
 elseif stage==410 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=411
 elseif stage==411 and ticks>=ready and s.artworks==0 then shot(s,'vvinyl-no-cover',412)
 elseif stage==412 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=413
 elseif stage==413 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',15);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',384);native.obs_data_set_int(settings,'canvas_height',720);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Vertical Urban isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=421
 elseif stage==421 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Vertical Urban demand or square cover bounds');rasters=s.renders;shot(s,'vurban-start',422)
 elseif stage==422 then ready=ticks+15;stage=423
 elseif stage==423 and ticks>=ready then check(s.renders==rasters,'Vertical Urban reflections rerasterized');shot(s,'vurban-moved',424)
 elseif stage==424 then update('pro_light_intensity',0);stage=425
 elseif stage==425 and ticks>=ready then check(not s.animation,'Vertical Urban zero light retained animation');shot(s,'vurban-off',426)
 elseif stage==426 then update('pro_hue',120);stage=427
 elseif stage==427 and ticks>=ready then check(s.renders==rasters,'Vertical Urban hue rerasterized');shot(s,'vurban-hue',428)
 elseif stage==428 then update('pro_panel_opacity',50);stage=434
 elseif stage==434 and ticks>=ready then shot(s,'vurban-half',435)
 elseif stage==435 then update('pro_panel_opacity',0);stage=429
 elseif stage==429 and ticks>=ready then shot(s,'vurban-transparent',430)
 elseif stage==430 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=431
 elseif stage==431 and ticks>=ready and s.artworks==0 then shot(s,'vurban-no-cover',432)
 elseif stage==432 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=433
 elseif stage==433 and s.frames==0 and s.sessions==0 and not s.animation then
  settings=native.obs_data_create();native.obs_data_set_bool(settings,'demo_enabled',true);native.obs_data_set_int(settings,'pro_preset',16);native.obs_data_set_bool(settings,'artwork_enabled',true);native.obs_data_set_string(settings,'demo_image',script_path()..'fixture-red.png');native.obs_data_set_int(settings,'canvas_width',480);native.obs_data_set_int(settings,'canvas_height',720);native.obs_data_set_int(settings,'pro_light_intensity',100);source=native.obs_source_create_private('obs_playing_next_development','Vertical Liquid isolated',settings);native.obs_source_inc_active(source);ready=ticks+10;stage=441
 elseif stage==441 and ticks>=ready and s.frames==1 and s.artworks==1 then check(s.animation and s.sessions==0 and s.artworkWidth>0 and s.artworkHeight>0,'Vertical Liquid demand or square cover bounds');rasters=s.renders;shot(s,'vliquid-start',442)
 elseif stage==442 then ready=ticks+15;stage=443
 elseif stage==443 and ticks>=ready then check(s.renders==rasters,'Vertical Liquid reflections rerasterized');shot(s,'vliquid-moved',444)
 elseif stage==444 then update('pro_light_intensity',0);stage=445
 elseif stage==445 and ticks>=ready then check(not s.animation,'Vertical Liquid zero light retained animation');shot(s,'vliquid-off',446)
 elseif stage==446 then update('pro_hue',120);stage=447
 elseif stage==447 and ticks>=ready then check(s.renders==rasters,'Vertical Liquid hue rerasterized');shot(s,'vliquid-hue',448)
 elseif stage==448 then update('pro_panel_opacity',50);stage=454
 elseif stage==454 and ticks>=ready then shot(s,'vliquid-half',455)
 elseif stage==455 then update('pro_panel_opacity',0);stage=449
 elseif stage==449 and ticks>=ready then shot(s,'vliquid-transparent',450)
 elseif stage==450 then native.obs_data_set_bool(settings,'artwork_enabled',false);native.obs_source_update(source,settings);ready=ticks+5;stage=451
 elseif stage==451 and ticks>=ready and s.artworks==0 then shot(s,'vliquid-no-cover',452)
 elseif stage==452 then native.obs_source_dec_active(source);native.obs_source_release(source);source=nil;native.obs_data_release(settings);settings=nil;stage=453
 elseif stage==453 and s.frames==0 and s.sessions==0 and not s.animation then check(not s.animation,'Vertical Vinyl timer remained after hide');local f=assert(io.open(script_path()..'pro-pilot-result.txt','w'));f:write('PASS checks='..checks..(verticalOnly and '; pilot=VerticalGeometry; geometry_reset=all_PRO; captures=35' or '; pilot=NeonClub; static=1; intensity_reuses_raster=1; hue=1; pulse_shared=1; zero_stops_pulse=1; pingpong=1; classics_no_cover=1; captures=95'));f:close();obslua.timer_remove(safePulse);stage=20 end
 if stage==19 and ticks%10==0 then print('Final frames='..tostring(s.frames)..' sessions='..tostring(s.sessions))end
 if probe then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obslua.timer_remove(safePulse);error(err)end end
function script_load()obslua.timer_add(safePulse,100)end
function script_unload()obslua.timer_remove(safePulse);if source then native.obs_source_dec_active(source);native.obs_source_release(source)end;if settings then native.obs_data_release(settings)end end
