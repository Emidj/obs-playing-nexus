-- Allocation control: initialize the module, do not create a source/session.
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;
int obs_open_module(obs_module_t **module,const char *path,const char *data_path);
bool obs_init_module(obs_module_t *module);]]
local native=ffi.load('obs.dll')
local function run()
    obslua.timer_remove(run)
    local module=ffi.new('obs_module_t*[1]')
    assert(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0)
    assert(native.obs_init_module(module[0]))
    local file=assert(io.open(script_path()..'source-lifecycle-result.txt','w'))
    file:write('PASS module load only; no source or session created');file:close()
end
function script_load() obslua.timer_add(run,800) end
function script_unload() obslua.timer_remove(run) end
