-- User-review scene in an isolated portable OBS. No diagnostic DLL hooks.
local obs=obslua
local ffi=require('ffi')
ffi.cdef[[typedef struct obs_module obs_module_t; int obs_open_module(obs_module_t**,const char*,const char*); bool obs_init_module(obs_module_t*);]]
local native=ffi.load('obs.dll')
local function add(scene,name,pro,direction,width,height,x,y)
 local source=obs.obs_get_source_by_name(name)
 if not source then
  local settings=obs.obs_data_create()
  obs.obs_data_set_string(settings,'provider','virtualdj')
  obs.obs_data_set_bool(settings,'demo_enabled',true)
  obs.obs_data_set_string(settings,'demo_title','Mind Still (Original Mix)')
  obs.obs_data_set_string(settings,'demo_artist','Sonny Fodera')
  obs.obs_data_set_string(settings,'demo_image',script_path()..((pro==1 or pro==4 or pro==5 or pro==6 or pro==7 or pro==8 or pro==9 or pro==10 or pro==11 or pro==12 or pro==13 or pro==14 or pro==15 or pro==16) and 'preview-demo-cover.png' or 'preview-profile.png'))
  obs.obs_data_set_bool(settings,'show_existing',true)
  obs.obs_data_set_int(settings,'pro_preset',pro==16 and 16 or pro==15 and 15 or pro==14 and 14 or pro==13 and 13 or pro==12 and 12 or pro==11 and 11 or pro==10 and 10 or pro==9 and 9 or pro==8 and 8 or pro==7 and 7 or pro==6 and 6 or pro==5 and 5 or pro==4 and 4 or pro==1 and 1 or direction==2 and 3 or 2)
  obs.obs_data_set_int(settings,'text_layout',direction)
  obs.obs_data_set_int(settings,'canvas_width',width)
  obs.obs_data_set_int(settings,'canvas_height',height)
  obs.obs_data_set_int(settings,'text_margin',28)
  obs.obs_data_set_int(settings,'text_cover_size',pro==16 and 242 or pro==14 and 230 or (pro==1 or pro==4 or pro==5 or pro==6 or pro==7 or pro==8 or pro==9 or pro==10 or pro==11 or pro==12 or pro==13 or pro==14 or pro==15 or pro==16) and 208 or 120)
  obs.obs_data_set_int(settings,'text_line_gap',pro==7 and 0 or 12)
  obs.obs_data_set_int(settings,'text_artist_color',(pro==8 or pro==9 or pro==11 or pro==13 or pro==16) and 0xffffe600 or (pro==7 or pro==15) and 0xff23d8ff or (pro==4 or pro==14) and 0xff50d7ff or pro==5 and 0xffb8a091 or 0xffb400ff)
  obs.obs_data_set_int(settings,'pro_light_intensity',100)
  if pro==1 or pro==4 or pro==5 or pro==6 or pro==7 or pro==8 or pro==9 or pro==10 or pro==11 or pro==12 or pro==13 or pro==14 or pro==15 or pro==16 then
   local font=obs.obs_data_create();obs.obs_data_set_string(font,'face',pro==7 and 'Segoe Print' or 'Bahnschrift SemiCondensed');obs.obs_data_set_int(font,'size',56);obs.obs_data_set_int(font,'flags',1)
   obs.obs_data_set_obj(settings,'text_title_font',font);obs.obs_data_set_int(font,'size',44);obs.obs_data_set_obj(settings,'text_artist_font',font);obs.obs_data_release(font)
   obs.obs_data_set_int(settings,'title_overflow',4)
  end
  obs.obs_data_set_bool(settings,'artwork_enabled',true)
  obs.obs_data_set_string(settings,'streamer_image',script_path()..((pro==1 or pro==4 or pro==5 or pro==6 or pro==7 or pro==8 or pro==9 or pro==10 or pro==11 or pro==12 or pro==13 or pro==14 or pro==15 or pro==16) and 'preview-demo-cover.png' or 'preview-profile.png'))
  source=obs.obs_source_create('obs_playing_next_development',name,settings,nil)
  obs.obs_data_release(settings)
  assert(source,'Cannot create preview source')
  local item=obs.obs_scene_add(scene,source)
  local position=obs.vec2();position.x=x;position.y=y
  obs.obs_sceneitem_set_pos(item,position)
 end
 obs.obs_source_release(source)
end
local function setup()
 obs.timer_remove(setup)
 local module=ffi.new('obs_module_t*[1]')
 assert(native.obs_open_module(module,script_path()..'obs-playing-next.dll',script_path())==0,'Cannot open preview module')
 assert(native.obs_init_module(module[0]),'Cannot initialize preview module')
 local scene=obs.obs_scene_create('Fase 17 - revision visual')
 add(scene,'Neon Club - piloto editable',1,1,960,367,40,35)
 add(scene,'Solo texto horizontal - editable',0,1,960,180,40,440)
 add(scene,'Solo texto vertical - editable',0,2,320,560,1080,50)
 local source=obs.obs_scene_get_source(scene)
 obs.obs_frontend_set_current_scene(source)
 obs.obs_frontend_save()
 obs.obs_scene_release(scene)
 obs.script_log(obs.LOG_INFO,'Phase 17 user preview ready; static fixture, not a live DJ session')
end
function script_load()obs.timer_add(setup,1000)end
function script_unload()obs.timer_remove(setup)end
