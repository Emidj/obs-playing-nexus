-- Test-only script. Load only in the isolated portable OBS audit collection.
local ffi = require('ffi')
ffi.cdef[[
int playing_core_smoke_test(void); int playing_provider_smoke_test(void);
int playing_history_probe_start(const char *filename);
int playing_history_probe_step(void); void playing_history_probe_stop(void);
const char *playing_history_probe_error(void);
]]
local probe = ffi.load(script_path() .. 'playing-core-probe.dll')
local ticks = 0
local history_path
local function history_tick()
    ticks = ticks + 1
    -- OBS script_load and timer callbacks can run on different threads. Create,
    -- service and release the owner-bound registry from this same callback.
    if ticks == 1 then
        assert(probe.playing_history_probe_start(history_path) == 0, 'Native history probe start failed')
    end
    local result = probe.playing_history_probe_step()
    if result == 1 and ticks < 100 then return end
    obslua.timer_remove(history_tick)
    probe.playing_history_probe_stop()
    local file = assert(io.open(script_path() .. 'history-probe-result.txt', 'w'))
    file:write('VirtualDJ native history OBS result=' .. result .. '; ticks=' .. ticks
        .. '; detail=' .. ffi.string(probe.playing_history_probe_error()) .. '\n')
    file:close()
    assert(result == 0, 'VirtualDJ native history OBS test failed: ' .. result)
end
function script_load()
    local result = probe.playing_core_smoke_test()
    local file = assert(io.open(script_path() .. 'core-probe-result.txt', 'w'))
    file:write('Playing Core DLL smoke result=' .. result .. '\n')
    file:close()
    assert(result == 0, 'Playing Core DLL smoke test failed: ' .. result)
    local provider_result = probe.playing_provider_smoke_test()
    local provider_file = assert(io.open(script_path() .. 'provider-probe-result.txt', 'w'))
    provider_file:write('Provider Registry DLL smoke result=' .. provider_result .. '\n')
    provider_file:close()
    assert(provider_result == 0, 'Provider Registry DLL smoke test failed: ' .. provider_result)
    history_path = script_path() .. 'history-probe-tracklist.txt'
    local history_file = assert(io.open(history_path, 'wb'))
    history_file:write('21:03 : Björk - Jóga 🎵\r\n'); history_file:close()
    obslua.timer_add(history_tick, 25)
end
function script_unload()
    obslua.timer_remove(history_tick)
    probe.playing_history_probe_stop()
end
