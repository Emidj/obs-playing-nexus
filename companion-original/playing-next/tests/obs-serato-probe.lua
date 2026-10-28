-- Test-only loopback peer; native production registration still requires Bonjour.
local ffi=require('ffi')
ffi.cdef[[
int playing_serato_probe_start(void); unsigned playing_serato_probe_port(void);
int playing_serato_probe_step(unsigned stage); void playing_serato_probe_stop(void);
]]
local probe=ffi.load(script_path()..'playing-core-probe.dll')
local config,ticks,stage,stages,published
local function tick()
    ticks=ticks+1
    if ticks==1 then assert(probe.playing_serato_probe_start()==0,'Serato start failed') end
    local port=probe.playing_serato_probe_port()
    if port>0 and not published then
        local file=assert(io.open(config.root..'/port.txt','w')); file:write(port); file:close(); published=true
    end
    local ack=io.open(config.root..'/ack.txt','r')
    local value=ack and tonumber(ack:read('*a')); if ack then ack:close() end
    local result=1
    -- Keep servicing authorization before the external peer publishes ack 0.
    if value==stage or stage==0 then
        result=probe.playing_serato_probe_step(stage)
        if value~=stage then result=1 end
    end
    if result==2 then
        stages=stages..stage..','; stage=stage+1
        local file=assert(io.open(config.root..'/command.txt','w')); file:write(stage); file:close(); return
    end
    if result==1 and ticks<400 then return end
    obslua.timer_remove(tick); probe.playing_serato_probe_stop()
    local file=assert(io.open(config.report,'w'))
    file:write('Serato native local OBS result='..result..'; ticks='..ticks..'; stages='..stages..stage..'; shared_consumers=2\n'); file:close()
    assert(result==0,'Serato OBS failed '..result)
end
function script_load()
    local file=assert(io.open(script_path()..'serato-probe-config.json','rb'))
    local data=obslua.obs_data_create_from_json(file:read('*a')); file:close(); assert(data)
    config={root=obslua.obs_data_get_string(data,'root'),report=obslua.obs_data_get_string(data,'report')}
    obslua.obs_data_release(data); ticks,stage,stages,published=0,0,'',false
    obslua.timer_add(tick,25)
end
function script_unload() obslua.timer_remove(tick); probe.playing_serato_probe_stop() end
