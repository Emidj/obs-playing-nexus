-- Test-only live bridge from external authenticated Receiver, portable OBS only.
local ffi=require('ffi')
ffi.cdef[[int playing_companion_probe_start(void);
int playing_companion_probe_step(const char *frame,unsigned size,unsigned stage);
int playing_companion_probe_stop(void);]]
local probe=ffi.load(script_path()..'playing-core-probe.dll')
local root,ticks,stage,stages
local function tick()
    ticks=ticks+1
    if ticks==1 then assert(probe.playing_companion_probe_start()==0,'Companion probe start') end
    local file=io.open(root..'/bridge.pnx','rb')
    local frame=file and file:read(65537); if file then file:close() end
    local result=1
    if frame and #frame<=65536 then result=probe.playing_companion_probe_step(frame,#frame,stage) end
    if result==2 then
        stages=stages..stage..','; stage=stage+1
        local command=assert(io.open(root..'/obs.command','wb')); command:write(stage); command:close()
        if stage<3 then return end
        result=probe.playing_companion_probe_stop()
    end
    if result==1 and ticks<200 then return end
    obslua.timer_remove(tick); probe.playing_companion_probe_stop()
    local report=assert(io.open(root..'/obs-result.txt','w'))
    report:write('Companion OBS result='..result..'; stages='..stages..'; shared_consumers=2; ticks='..ticks..'\n'); report:close()
    assert(result==0,'Companion OBS bridge failed '..result)
end
function script_load()
    local f=assert(io.open(script_path()..'companion-probe-root.txt','r')); root=f:read('*a'):gsub('%s+$',''); f:close()
    ticks,stage,stages=0,0,''; obslua.timer_add(tick,50)
end
function script_unload() obslua.timer_remove(tick); probe.playing_companion_probe_stop() end
