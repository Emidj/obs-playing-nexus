-- Matching control: same extra Lua slot and OBS library, no new module/source.
local ffi=require('ffi')
local native=ffi.load('obs.dll')
local function run()
    obslua.timer_remove(run)
    assert(native~=nil)
    local file=assert(io.open(script_path()..'source-lifecycle-result.txt','w'))
    file:write('PASS original baseline; no Playing Next module loaded');file:close()
end
function script_load() obslua.timer_add(run,800) end
function script_unload() obslua.timer_remove(run) end
