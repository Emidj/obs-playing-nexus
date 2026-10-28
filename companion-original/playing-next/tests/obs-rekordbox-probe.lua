-- Test-only. Load in the isolated portable collection beside the probe DLL.
local ffi=require('ffi')
ffi.cdef[[
int playing_rekordbox_probe_start(const char *filename, const char *library);
int playing_rekordbox_probe_step(int expect_wal);
void playing_rekordbox_probe_stop(void);
const char *playing_rekordbox_probe_error(void);
]]
local probe=ffi.load(script_path()..'playing-core-probe.dll')
local config, ticks, stage, initial_ticks
local function tick()
    ticks=ticks+1
    if ticks==1 then
        assert(probe.playing_rekordbox_probe_start(config.database,config.library)==0,
            'rekordbox native probe start failed: '..ffi.string(probe.playing_rekordbox_probe_error()))
    end
    local result=probe.playing_rekordbox_probe_step(stage)
    if result==2 then
        initial_ticks=ticks; stage=1
        local marker=assert(io.open(config.ready,'w'))
        marker:write('Initial encrypted history PASS; ticks='..ticks..'\n'); marker:close()
        return
    end
    if result==1 and ticks<360 then return end
    obslua.timer_remove(tick)
    probe.playing_rekordbox_probe_stop()
    local report=assert(io.open(config.report,'w'))
    report:write('rekordbox native history OBS result='..result..'; ticks='..ticks
        ..'; initial_ticks='..tostring(initial_ticks)..'; detail='
        ..ffi.string(probe.playing_rekordbox_probe_error())..'\n'); report:close()
    assert(result==0,'rekordbox native OBS history/WAL probe failed: '..result)
end
function script_load()
    local file=assert(io.open(script_path()..'rekordbox-probe-config.json','rb'))
    local data=obslua.obs_data_create_from_json(file:read('*a')); file:close()
    assert(data,'Invalid isolated probe configuration')
    config={database=obslua.obs_data_get_string(data,'database'),library=obslua.obs_data_get_string(data,'library'),
        ready=obslua.obs_data_get_string(data,'ready'),report=obslua.obs_data_get_string(data,'report')}
    obslua.obs_data_release(data)
    ticks, stage=0,0
    obslua.timer_add(tick,25)
end
function script_unload()
    obslua.timer_remove(tick)
    probe.playing_rekordbox_probe_stop()
end
