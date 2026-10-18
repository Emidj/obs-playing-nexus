-- Developer-only: isolated portable collection, no source registered/installed.
local ffi=require('ffi')
ffi.cdef[[int playing_visual_probe_start(void);int playing_visual_probe_step(void);const char *playing_visual_probe_evidence(void);]]
local probe=ffi.load(script_path()..'playing-visual-probe.dll')
local ticks=0
local function pulse()
    ticks=ticks+1
    if ticks==4 then assert(probe.playing_visual_probe_start()==0,'Visual probe start') end
    if ticks<4 then return end
    local result=probe.playing_visual_probe_step()
    if result==1 and ticks<100 then return end
    obslua.timer_remove(pulse)
    local file=assert(io.open(script_path()..'visual-probe-result.txt','w'))
    file:write(ffi.string(probe.playing_visual_probe_evidence()));file:close()
    assert(result==0,'Visual probe failed: '..ffi.string(probe.playing_visual_probe_evidence()))
end
function script_load() obslua.timer_add(pulse,200) end
function script_unload() obslua.timer_remove(pulse) end
