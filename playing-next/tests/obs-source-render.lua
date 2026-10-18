-- Development hooks only; private sources and fixtures within isolated work.
local ffi=require('ffi')
ffi.cdef[[
typedef struct obs_module obs_module_t; typedef struct obs_source obs_source_t; typedef struct obs_data obs_data_t;
typedef struct obs_scene obs_scene_t; typedef struct obs_scene_item obs_sceneitem_t;
typedef struct obs_properties obs_properties_t;typedef struct obs_property obs_property_t;
int obs_open_module(obs_module_t **module,const char *path,const char *data_path); bool obs_init_module(obs_module_t *module);
obs_data_t *obs_data_create(void); void obs_data_set_string(obs_data_t*,const char*,const char*); void obs_data_set_int(obs_data_t*,const char*,int64_t); void obs_data_release(obs_data_t*);
obs_source_t *obs_source_create_private(const char*,const char*,obs_data_t*);void obs_source_release(obs_source_t*);
void obs_source_inc_active(obs_source_t*);void obs_source_dec_active(obs_source_t*);void obs_source_update(obs_source_t*,obs_data_t*);
uint32_t obs_source_get_width(obs_source_t*);uint32_t obs_source_get_height(obs_source_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);void playing_source_test_capture(obs_source_t*);
void playing_source_test_remote_fixture(const char*);void playing_source_test_remote_publish(int);
obs_scene_t *obs_scene_create_private(const char*);obs_source_t *obs_scene_get_source(const obs_scene_t*);void obs_scene_release(obs_scene_t*);
obs_sceneitem_t *obs_scene_add(obs_scene_t*,obs_source_t*);bool obs_sceneitem_set_visible(obs_sceneitem_t*,bool);void obs_sceneitem_remove(obs_sceneitem_t*);
obs_source_t *obs_get_output_source(uint32_t);void obs_set_output_source(uint32_t,obs_source_t*);
obs_properties_t *obs_source_properties(const obs_source_t*);void obs_properties_destroy(obs_properties_t*);obs_property_t *obs_properties_get(obs_properties_t*,const char*);
void obs_properties_apply_settings(obs_properties_t*,obs_data_t*);bool obs_property_visible(obs_property_t*);size_t obs_property_list_item_count(obs_property_t*);
]]
local native=ffi.load('obs.dll')
local probe,first,second,settings
local scene,sceneSource,original,item1,item2
local stage,ticks,checks=0,0,0
local lastHash,uploads,refreshes,waitTick
local overflowMode,overflowHash,layoutRevision
local safePulse
local longTitle='Long Unicode 日本語 🎵 title with many words across a narrow rectangle'
local function check(value,why) checks=checks+1;assert(value,why) end
local function write(text) local f=assert(io.open(script_path()..'source-history.txt','wb'));f:write(text);f:close() end
local function read()
    local buffer=ffi.new('char[4096]');local size=probe.playing_source_test_read(buffer,4096)
    if size==0 then return {} end
    local text=ffi.string(buffer,size)
    local result={}
    for key,value in text:gmatch('"([%w]+)":([%d]+)') do result[key]=tonumber(value) end
    result.title=text:match('"title":"(.-)"');result.hash=text:match('"captureHash":"(.-)"');result.timer=text:match('"timer":true')~=nil
    return result
end
local function pulse()
    ticks=ticks+1
    if ticks>110 then obslua.timer_remove(safePulse);error('Native source timed out at stage '..stage) end
    if stage==0 then
        local module=ffi.new('obs_module_t*[1]')
        check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open')
        check(native.obs_init_module(module[0]),'Module init')
        probe=ffi.load(script_path()..'obs-playing-next.dll')
        write('21:05 : Björk - Jóga 🎵\r\n')
        settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','virtualdj');native.obs_data_set_int(settings,'mode',1)
        native.obs_data_set_string(settings,'history',script_path()..'source-history.txt')
        first=native.obs_source_create_private('obs_playing_next_development','Native first',settings)
        second=native.obs_source_create_private('obs_playing_next_development','Native second',settings)
        check(first~=nil and second~=nil,'Private sources')
        local props=native.obs_source_properties(first)
        check(native.obs_property_list_item_count(native.obs_properties_get(props,'provider'))==4,'Provider selector incomplete')
        check(native.obs_property_list_item_count(native.obs_properties_get(props,'normal_preset'))==27 and native.obs_property_list_item_count(native.obs_properties_get(props,'normal_transition'))==3,'Verified NORMAL/motion selector incomplete')
        check(native.obs_property_list_item_count(native.obs_properties_get(props,'title_overflow'))==5 and native.obs_property_list_item_count(native.obs_properties_get(props,'artist_overflow'))==5,'Overflow selectors incomplete')
        local ui=native.obs_data_create();native.obs_data_set_int(ui,'preset_family',0);native.obs_data_set_int(ui,'preset_style',0);native.obs_data_set_string(ui,'provider','network');native.obs_properties_apply_settings(props,ui)
        check(not native.obs_property_visible(native.obs_properties_get(props,'title_minimum')),'Minimum shown outside AutoFit')
        native.obs_data_set_int(ui,'editor_mode',1);native.obs_data_set_int(ui,'title_overflow',3);native.obs_data_set_int(ui,'artist_overflow',3);native.obs_properties_apply_settings(props,ui)
        check(native.obs_property_visible(native.obs_properties_get(props,'title_minimum')) and native.obs_property_visible(native.obs_properties_get(props,'artist_minimum')),'AutoFit minimum hidden')
        check(native.obs_property_visible(native.obs_properties_get(props,'pair_receiver')) and not native.obs_property_visible(native.obs_properties_get(props,'history')),'Remote UI visibility')
        native.obs_data_set_string(ui,'provider','serato');native.obs_properties_apply_settings(props,ui)
        check(native.obs_property_visible(native.obs_properties_get(props,'mode')),'Serato reading selector remains visible')
        native.obs_data_set_string(ui,'provider','rekordbox');native.obs_data_set_int(ui,'mode',1);native.obs_properties_apply_settings(props,ui)
        check(native.obs_property_visible(native.obs_properties_get(props,'history')) and not native.obs_property_visible(native.obs_properties_get(props,'encoding')),'rekordbox history UI')
        native.obs_data_set_int(ui,'editor_mode',2);native.obs_data_set_int(ui,'preset_style',1);native.obs_data_set_int(ui,'normal_preset',1);native.obs_properties_apply_settings(props,ui)
        check(native.obs_property_visible(native.obs_properties_get(props,'normal_gap')) and not native.obs_property_visible(native.obs_properties_get(props,'canvas_height')) and not native.obs_property_visible(native.obs_properties_get(props,'title_minimum')),'NORMAL motion/basic UI visibility')
        native.obs_data_release(ui);native.obs_properties_destroy(props)
        scene=native.obs_scene_create_private('Native integration scene');sceneSource=native.obs_scene_get_source(scene)
        item1=native.obs_scene_add(scene,first);item2=native.obs_scene_add(scene,second)
        original=native.obs_get_output_source(0);native.obs_set_output_source(0,sceneSource);stage=1
    else
        local s=read()
        if stage==1 and s.frames==2 then
            check(s.sessions==1 and s.consumers==2,'Shared DJ session');check(s.title=='Jóga 🎵','Unicode track');check(s.renders==2,'Shared CPU cache')
            probe.playing_source_test_capture(first);stage=2
        elseif stage==2 and s.captures==1 then
            check(s.capturePixels>0,'Real video_render drew no pixels');check(s.uploads==2,'GPU uploads');lastHash=s.hash;uploads=s.uploads
            probe.playing_source_test_capture(second);stage=3
        elseif stage==3 and s.captures==2 then
            check(s.hash==lastHash and s.uploads==uploads,'Second source repeated uploads/changed output')
            probe.playing_source_test_capture(sceneSource);stage=31
        elseif stage==31 and s.captures==3 then
            check(s.capturePixels>0 and s.uploads==uploads,'Scene render missing/shared cache broken')
            write('21:06 : Bjork - Changed song\r\n');stage=4
        elseif stage==4 and s.title=='Changed song' then probe.playing_source_test_capture(first);stage=5
        elseif stage==5 and s.captures==4 then
            check(s.hash~=lastHash and s.capturePixels>0,'Track change not drawn')
            native.obs_sceneitem_set_visible(item1,false);stage=6
        elseif stage==6 and s.consumers==1 then
            check(s.frames==1 and s.sessions==1,'Scene hide released shared peer');native.obs_sceneitem_set_visible(item2,false);stage=7
        elseif stage==7 and s.consumers==0 then
            check(s.frames==0 and s.sessions==0 and not s.timer,'Last consumer did not stop')
            refreshes=s.refreshes;waitTick=ticks;stage=8
        elseif stage==8 and ticks-waitTick>=3 then
            check(s.refreshes==refreshes and s.uploads==0,'Idle refresh/GPU cache remained')
            native.obs_sceneitem_set_visible(item1,true);stage=9
        elseif stage==9 and s.frames==1 then
            check(s.title=='Changed song','Reactivation lost track');native.obs_source_update(first,settings);waitTick=ticks;stage=10
        elseif stage==10 and s.frames==1 and ticks-waitTick>=3 then
            check(s.title=='Changed song','Same settings left publication blank')
            write('21:07 : Artist - '..longTitle..'\r\n');stage=101
        elseif stage==101 and s.title==longTitle then
            layoutRevision=s.trackRevision
            native.obs_data_set_int(settings,'canvas_width',800);native.obs_data_set_int(settings,'canvas_height',240)
            native.obs_data_set_int(settings,'title_width',180);native.obs_data_set_int(settings,'title_height',120)
            native.obs_data_set_int(settings,'artist_y',170);native.obs_data_set_int(settings,'title_align',1)
            overflowMode=0;native.obs_data_set_int(settings,'title_overflow',overflowMode);native.obs_source_update(first,settings)
            -- Video-source updates are applied on a subsequent graphics tick.
            waitTick=ticks;stage=102
        elseif stage==102 and s.frames==1 and ticks-waitTick>=2 then
            check(native.obs_source_get_width(first)==800 and native.obs_source_get_height(first)==240,'Dynamic source dimensions')
            check(s.trackRevision==layoutRevision and s.sessions==1,'Layout replaced Core/session')
            probe.playing_source_test_capture(first);stage=103
        elseif stage==103 and s.captures==5+overflowMode then
            check(s.capturePixels>0,'Overflow mode rendered empty text')
            if overflowHash then check(s.hash~=overflowHash,'Overflow modes drew identical long text') end
            overflowHash=s.hash;overflowMode=overflowMode+1
            if overflowMode<4 then native.obs_data_set_int(settings,'title_overflow',overflowMode);native.obs_source_update(first,settings);waitTick=ticks;stage=102
            else stage=104 end
        elseif stage==104 then
            native.obs_data_set_int(settings,'canvas_width',0);native.obs_data_set_int(settings,'canvas_height',3000);native.obs_source_update(first,settings)
            waitTick=ticks;stage=1041
        elseif stage==1041 and ticks-waitTick>=2 then
            check(native.obs_source_get_width(first)==64 and native.obs_source_get_height(first)==2048,'Unsafe canvas dimensions not bounded')
            native.obs_data_set_int(settings,'canvas_width',800);native.obs_data_set_int(settings,'canvas_height',240);native.obs_source_update(first,settings)
            -- Exercise cancellation of commands queued before immediate destruction.
            for i=1,20 do
                local transient=native.obs_source_create_private('obs_playing_next_development','Transient',settings)
                check(transient~=nil,'Transient create');native.obs_source_inc_active(transient);native.obs_source_dec_active(transient);native.obs_source_release(transient)
            end
            native.obs_sceneitem_remove(item1);item1=nil;native.obs_sceneitem_remove(item2);item2=nil
            native.obs_source_release(first);first=nil;native.obs_source_release(second);second=nil
            native.obs_data_release(settings);settings=nil;stage=11
        elseif stage==11 and s.consumers==0 and s.sessions==0 then
            check(s.frames==0 and not s.timer,'Destroy retained readers')
            probe.playing_source_test_remote_fixture(script_path()..'source-receiver.pcs');stage=12
        elseif stage==12 and s.remoteReady==1 then
            settings=native.obs_data_create();native.obs_data_set_string(settings,'provider','network')
            native.obs_data_set_string(settings,'receiver_profile',script_path()..'source-receiver.pcs')
            first=native.obs_source_create_private('obs_playing_next_development','Remote first',settings)
            second=native.obs_source_create_private('obs_playing_next_development','Remote second',settings)
            item1=native.obs_scene_add(scene,first);item2=native.obs_scene_add(scene,second);stage=13
        elseif stage==13 and s.frames==2 and s.title=='Remote 🎵' then
            check(s.remoteClients==1 and s.sessions==1 and not s.timer,'OBS remote shared connection/event scheduling')
            probe.playing_source_test_capture(sceneSource);stage=14
        elseif stage==14 and s.captures==9 then
            check(s.capturePixels>0,'TLS track not drawn in scene');lastHash=s.hash
            probe.playing_source_test_remote_publish(1);stage=15
        elseif stage==15 and s.title=='Remote next' then probe.playing_source_test_capture(sceneSource);stage=16
        elseif stage==16 and s.captures==10 then
            check(s.hash~=lastHash,'TLS track change not drawn');probe.playing_source_test_remote_publish(2);stage=17
        elseif stage==17 and s.frames==0 then
            check(s.sessions==1 and s.remoteClients==1,'Pause disconnected session');probe.playing_source_test_remote_publish(3);stage=18
        elseif stage==18 and s.title=='Remote resumed' then probe.playing_source_test_remote_publish(4);stage=19
        elseif stage==19 and s.frames==0 and s.remoteClients==0 then
            check(not s.timer,'Revoked remote is polling');native.obs_sceneitem_remove(item1);item1=nil;native.obs_sceneitem_remove(item2);item2=nil
            native.obs_source_release(first);first=nil;native.obs_source_release(second);second=nil
            native.obs_data_release(settings);settings=nil;stage=20
        elseif stage==20 and s.consumers==0 and s.sessions==0 then
            check(s.frames==0 and not s.timer,'Remote destroy retained demand')
            native.obs_set_output_source(0,original);native.obs_source_release(original);original=nil;native.obs_scene_release(scene);scene=nil
            local f=assert(io.open(script_path()..'source-render-result.txt','w'));f:write('PASS checks='..checks..'; scene=1; properties=1; local_and_TLS=1; two_consumers=1; paused_and_revoked=1; inactive_timer=0; sources_released=1');f:close()
            obslua.timer_remove(safePulse);stage=21
        end
    end
    if probe then probe.playing_source_test_query() end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obslua.timer_remove(safePulse);error(err)end end
function script_load() obslua.timer_add(safePulse,200) end
function script_unload()
    obslua.timer_remove(safePulse)
    if scene then native.obs_set_output_source(0,original);if original then native.obs_source_release(original) end;native.obs_scene_release(scene) end
    if first then native.obs_source_release(first) end
    if second then native.obs_source_release(second) end
    if settings then native.obs_data_release(settings) end
end
