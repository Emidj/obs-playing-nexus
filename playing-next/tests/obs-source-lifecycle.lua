-- Developer lifecycle test; no DJ metadata or product configuration.
local obs = obslua
local ffi = require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;
int obs_open_module(obs_module_t **module, const char *path, const char *data_path);
bool obs_init_module(obs_module_t *module);]]
local native = ffi.load('obs.dll')
local done = false
local function run()
    obs.timer_remove(run)
    local module = ffi.new('obs_module_t *[1]')
    assert(native.obs_open_module(module, script_path() .. 'obs-playing-next.dll', script_path()) == 0, 'Module open failed')
    assert(native.obs_init_module(module[0]), 'Module initialization failed')
    assert(obs.obs_source_get_display_name('obs_playing_next_development') == 'OBS Playing NEXUS', 'Source display name mismatch')
    local source = obs.obs_source_create_private('obs_playing_next_development', 'Playing lifecycle test', nil)
    assert(source ~= nil, 'Native source unavailable')
    assert(obs.obs_source_get_width(source) == 640, 'Width mismatch')
    assert(obs.obs_source_get_height(source) == 120, 'Height mismatch')
    local properties = obs.obs_source_properties(source)
    for _, key in ipairs({'text_title_font', 'text_artist_font', 'text_title_color', 'text_artist_color'}) do
        local property = obs.obs_properties_get(properties, key)
        assert(property ~= nil and obs.obs_property_visible(property), 'Manual customization hidden: ' .. key)
    end
    assert(obs.obs_properties_get(properties, 'preset_save_as') ~= nil, 'Direct Save As missing')
    assert(obs.obs_properties_get(properties, 'performance_profile') == nil, 'Removed performance selector remains')
    assert(obs.obs_properties_get(properties, 'streamer_image') == nil, 'Manual image is still a text field')
    assert(obs.obs_properties_get(properties, 'choose_streamer_image') ~= nil, 'Manual image file button missing')
    local settings = obs.obs_source_get_settings(source)
    assert(obs.obs_data_get_int(settings, 'performance_profile') == 1, 'Balanced default changed')
    for _, mode in ipairs({0, 1, 2, 0}) do
        obs.obs_data_set_int(settings, 'editor_mode', mode)
        obs.obs_properties_apply_settings(properties, settings)
        assert(obs.obs_property_visible(obs.obs_properties_get(properties, 'title_outline_width')) == (mode == 1), 'Advanced mode visibility')
        assert(obs.obs_property_visible(obs.obs_properties_get(properties, 'title_width')) == (mode == 2), 'Layout mode visibility')
        assert(obs.obs_property_visible(obs.obs_properties_get(properties, 'text_title_font')), 'Mode hid primary typography')
        assert(obs.obs_source_get_width(source) == 640 and obs.obs_source_get_height(source) == 120, 'Editor mode changed geometry')
    end
    -- Shared settings repeat get_defaults without replacing default font objects.
    local peer = obs.obs_source_create_private('obs_playing_next_development', 'Playing shared settings peer', settings)
    assert(peer ~= nil and obs.obs_source_get_width(peer) == 640 and obs.obs_source_get_height(peer) == 120, 'Shared settings source failed')
    obs.obs_source_release(peer)
    obs.obs_data_release(settings)
    obs.obs_properties_destroy(properties)
    obs.obs_source_inc_active(source)
    obs.obs_source_dec_active(source)
    obs.obs_source_release(source)
    local file = assert(io.open(script_path() .. 'source-lifecycle-result.txt', 'w'))
    file:write('PASS native source create/dimensions/activate/deactivate/release; shared settings peer; display name OBS Playing NEXUS')
    file:close()
    done = true
end
function script_load() obs.timer_add(run, 800) end
function script_unload() if not done then obs.timer_remove(run) end end
