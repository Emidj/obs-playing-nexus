-- Isolated OBS profile with no Twitch account. No credentials or HTTP fixtures.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t;
int obs_open_module(obs_module_t**,const char*,const char*);bool obs_init_module(obs_module_t*);
void playing_source_test_query(void);int playing_source_test_read(char*,int);]]
local native=ffi.load('obs.dll')
local probe,source,settings
local stage,ticks,checks,firstGeneration,hiddenAt=0,0,0,0,0
local safePulse
local function check(ok,why)checks=checks+1;assert(ok,why)end
local function pulse()
 ticks=ticks+1;assert(ticks<115,'Profile retry timed out at stage '..stage)
 if stage==0 then
  local module=ffi.new('obs_module_t*[1]')
  check(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Module open')
  check(native.obs_init_module(module[0]),'Module init');probe=ffi.load(script_path()..'obs-playing-next.dll')
  settings=obs.obs_data_create();obs.obs_data_set_bool(settings,'demo_enabled',true)
  obs.obs_data_set_bool(settings,'artwork_enabled',true);obs.obs_data_set_bool(settings,'artwork_fallback',true)
  obs.obs_data_set_bool(settings,'twitch_profile_auto',true)
  source=obs.obs_source_create_private('obs_playing_next_development','Profile retry fixture',settings)
  check(source~=nil,'Source');obs.obs_source_inc_active(source);stage=1
 else
  local b=ffi.new('char[4096]');local n=probe.playing_source_test_read(b,4096);local s={}
  if n>0 then for k,v in ffi.string(b,n):gmatch('"([%w]+)":(%d+)')do s[k]=tonumber(v)end end
  if stage==1 and s.profileFailures==1 then
   firstGeneration=s.profileGeneration;check(s.artworks==0,'No account invented an image');stage=2
  elseif stage==2 and s.profileFailures==2 then
   check(s.profileGeneration==firstGeneration+1,'First failed profile lookup was never retried')
   obs.obs_source_dec_active(source);hiddenAt=ticks;stage=3
  elseif stage==3 and ticks-hiddenAt>=79 then
   check(s.consumers==0 and s.frames==0,'Hidden source retained demand')
   obs.obs_source_inc_active(source);stage=4
  elseif stage==4 and s.profileFailures==3 then
   check(s.profileGeneration==firstGeneration+2,'Hidden retry performed another lookup')
   obs.obs_source_dec_active(source);obs.obs_source_release(source);source=nil
   obs.obs_data_release(settings);settings=nil;stage=5
  elseif stage==5 and s.consumers==0 and s.frames==0 then
   local f=assert(io.open(script_path()..'profile-retry-result.txt','w'))
   f:write('PASS checks='..checks..'; initial_failure_retry=1; hidden_retry_no_lookup=1; reactivation=1; release_with_pending_retry=1; no_account=1');f:close()
   obs.timer_remove(safePulse);stage=6
  end
 end
 if probe then probe.playing_source_test_query()end
end
safePulse=function()local ok,err=pcall(pulse);if not ok then obs.timer_remove(safePulse);error(err)end end
function script_load()obs.timer_add(safePulse,200)end
function script_unload()
 obs.timer_remove(safePulse)
 if source then obs.obs_source_dec_active(source);obs.obs_source_release(source)end
 if settings then obs.obs_data_release(settings)end
end
