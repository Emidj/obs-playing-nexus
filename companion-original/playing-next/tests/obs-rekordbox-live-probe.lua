-- Test-only. Reads the separate synthetic process through the native worker.
local ffi=require('ffi')
ffi.cdef[[
int playing_rekordbox_live_probe_start(unsigned pid, unsigned long long module);
int playing_rekordbox_live_probe_step(unsigned stage);
void playing_rekordbox_live_probe_stop(void);
const char *playing_rekordbox_live_probe_error(void);
]]
local probe=ffi.load(script_path()..'playing-core-probe.dll')
local config, ticks, stage, stages
local function tick()
    ticks=ticks+1
    if ticks==1 then assert(probe.playing_rekordbox_live_probe_start(config.pid,config.module)==0,'live start failed') end
    local ack=io.open(config.root..'/ack.txt','r')
    local value=ack and tonumber(ack:read('*a')); if ack then ack:close() end
    local result=1
    if value==stage then result=probe.playing_rekordbox_live_probe_step(stage) end
    if result==2 then
        stages=stages..stage..','; stage=stage+1
        local file=assert(io.open(config.root..'/command.txt','w')); file:write(stage); file:close()
        return
    end
    if result==1 and ticks<300 then return end
    obslua.timer_remove(tick)
    probe.playing_rekordbox_live_probe_stop()
    local report=assert(io.open(config.report,'w'))
    report:write('rekordbox native automatic memory OBS result='..result..'; ticks='..ticks..'; stages='..stages
        ..tostring(stage)..'; shared_consumers=2; detail='..ffi.string(probe.playing_rekordbox_live_probe_error())..'\n'); report:close()
    assert(result==0,'live OBS fixture failed '..result)
end
function script_load()
    local file=assert(io.open(script_path()..'rekordbox-live-probe-config.json','rb'))
    local data=obslua.obs_data_create_from_json(file:read('*a')); file:close(); assert(data)
    config={pid=obslua.obs_data_get_int(data,'pid'),module=obslua.obs_data_get_int(data,'module'),
        root=obslua.obs_data_get_string(data,'root'),report=obslua.obs_data_get_string(data,'report')}
    obslua.obs_data_release(data); ticks,stage,stages=0,0,''
    obslua.timer_add(tick,25)
end
function script_unload()
    obslua.timer_remove(tick); probe.playing_rekordbox_live_probe_stop()
end
