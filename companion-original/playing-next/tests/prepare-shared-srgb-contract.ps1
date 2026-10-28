param([switch]$Apply)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$sharedRoot='C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays'
$folder=Join-Path $workspace 'outputs/phase12d-shared-color-contract'
$locks=@('sources.lock.json','renderer.lock.json')
$expected=@{}
foreach($name in $locks){foreach($row in (Get-Content (Join-Path $PSScriptRoot "visual-audit/$name") -Raw|ConvertFrom-Json)){$expected[$row.path]=$row.sha256}}
$paths=@('src/visual/visual-engine.hpp','src/visual/visual-engine.cpp','src/adapters/obs-visual-renderer.hpp','src/adapters/obs-visual-renderer.cpp')
$existingManifest=Join-Path $folder 'manifest.csv'
if(Test-Path -LiteralPath $existingManifest){
 $previous=@(Import-Csv -LiteralPath $existingManifest)
 if($previous.Count -eq 4 -and @($previous|Where-Object{(Get-FileHash -LiteralPath (Join-Path $sharedRoot $_.Path)).Hash -ne $_.After}).Count -eq 0){
  foreach($row in $previous){
   if($row.Path -notin $paths -or $expected[$row.Path] -ne $row.After){throw 'Applied shared contract differs from pinned sources'}
   $backupName=$row.Path.Replace('/','__')
   if((Get-FileHash -LiteralPath (Join-Path $folder ($backupName+'.before'))).Hash -ne $row.Before -or
      (Get-FileHash -LiteralPath (Join-Path $folder ($backupName+'.after'))).Hash -ne $row.After){throw 'Shared contract backup changed'}
  }
  Write-Output 'Shared contract already applied; four sources, pins and exact backups verified';return
 }
}
$planned=@{}
foreach($path in $paths){
 $target=Join-Path $sharedRoot $path
 if((Get-FileHash -LiteralPath $target).Hash -ne $expected[$path]){throw "Shared source changed before planned extension: $path"}
 $planned[$path]=[IO.File]::ReadAllText($target).Replace("`r`n","`n")
}
function Replace-Once([string]$path,[string]$before,[string]$after){
 $content=$planned[$path];$first=$content.IndexOf($before,[StringComparison]::Ordinal)
 if($first -lt 0 -or $content.IndexOf($before,$first+$before.Length,[StringComparison]::Ordinal) -ge 0){throw "Expected exactly one patch location: $path"}
 $planned[$path]=$content.Replace($before,$after)
}
Replace-Once $paths[0] 'struct Frame { QImage image; std::vector<TextMetrics> textMetrics; std::vector<ImagePtr> assets; };' @'
enum class RasterEncoding { Premultiplied, StraightSrgb };
struct Frame { QImage image; std::vector<TextMetrics> textMetrics; std::vector<ImagePtr> assets; RasterEncoding encoding=RasterEncoding::Premultiplied; };
'@
Replace-Once $paths[0] 'FramePtr prepareRaster(QImage image);' @'
FramePtr prepareRaster(QImage image);
	// Explicit straight sRGB pixels (e.g. Windows GDI). Existing scenes remain premultiplied.
	FramePtr prepareSrgbRaster(QImage image);
'@
Replace-Once $paths[0] 'FramePtr prepareImpl(const Scene &scene, bool textStrip);' @'
FramePtr prepareImpl(const Scene &scene, bool textStrip);
	FramePtr prepareRasterImpl(QImage image,bool srgb);
'@
Replace-Once $paths[1] "FramePtr VisualEngine::prepareRaster(QImage image)`n{" @'
FramePtr VisualEngine::prepareRaster(QImage image) {return prepareRasterImpl(std::move(image),false);}
FramePtr VisualEngine::prepareSrgbRaster(QImage image) {return prepareRasterImpl(std::move(image),true);}
FramePtr VisualEngine::prepareRasterImpl(QImage image,bool srgb)
{
'@
Replace-Once $paths[1] 'image.format()!=QImage::Format_RGBA8888_Premultiplied || image.bytesPerLine()!=image.width()*4' 'image.format()!=(srgb?QImage::Format_RGBA8888:QImage::Format_RGBA8888_Premultiplied) || image.bytesPerLine()!=image.width()*4'
Replace-Once $paths[1] 'QByteArray key("authored-raster-v1");QDataStream stream(&key,QIODevice::Append);stream<<image.size();' 'QByteArray key("authored-raster-v2");QDataStream stream(&key,QIODevice::Append);stream<<image.size()<<int(image.format());'
Replace-Once $paths[1] 'auto frame=std::make_shared<Frame>();frame->image=std::move(image);++stats_.renders;' 'auto frame=std::make_shared<Frame>();frame->image=std::move(image);frame->encoding=srgb?RasterEncoding::StraightSrgb:RasterEncoding::Premultiplied;++stats_.renders;'
Replace-Once $paths[2] 'QRectF cellLayer{0,0,1,1}; };' 'QRectF cellLayer{0,0,1,1};bool legacyAlpha=false; };'
Replace-Once $paths[2] 'void draw(const visual::FramePtr &frame, bool active = true, const DrawParams &params = {});' @'
void draw(const visual::FramePtr &frame, bool active = true, const DrawParams &params = {});
	// Generic authored solid quad with original source-alpha composition; no texture/cache.
	void drawSolidRect(const QRectF &box,const QColor &color,bool active=true);
'@
Replace-Once $paths[3] 'uniform float opacity;' "uniform float opacity;`nuniform float straight_alpha;"
Replace-Once $paths[3] " return color * opacity;`n}" " return straight_alpha>0.5 ? float4(color.rgb,color.a*opacity) : color*opacity;`n}"
Replace-Once $paths[3] 'BlendScope() { gs_blend_state_push(); gs_enable_blending(true); gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA); gs_enable_framebuffer_srgb(false); }' @'
BlendScope(bool straight=false,bool legacy=false,bool solid=false) {
		gs_blend_state_push();gs_enable_blending(true);
		if(straight&&!legacy)gs_blend_function_separate(GS_BLEND_SRCALPHA,GS_BLEND_INVSRCALPHA,GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
		else gs_blend_function(straight||solid?GS_BLEND_SRCALPHA:GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
		gs_enable_framebuffer_srgb(straight);
	}
'@
Replace-Once $paths[3] 'frame->image.format() != QImage::Format_RGBA8888_Premultiplied' 'frame->image.format() != (frame->encoding==visual::RasterEncoding::StraightSrgb?QImage::Format_RGBA8888:QImage::Format_RGBA8888_Premultiplied)'
Replace-Once $paths[3] "validateFrame(frame);`n`tconst bool region" @'
validateFrame(frame);
	const bool straight=frame->encoding==visual::RasterEncoding::StraightSrgb;
	if(frame->encoding!=visual::RasterEncoding::Premultiplied&&!straight)throw std::invalid_argument("Invalid raster encoding");
	if(straight&&(params.material!=0||params.dissolve!=0||!params.sourceRect.isNull()))throw std::invalid_argument("Straight sRGB raster cannot use material/cell effects");
	const bool region
'@
Replace-Once $paths[3] 'gs_effect_set_float(gs_effect_get_param_by_name(effect,"opacity"),params.opacity);' @'
gs_effect_set_float(gs_effect_get_param_by_name(effect,"opacity"),params.opacity);
		gs_effect_set_float(gs_effect_get_param_by_name(effect,"straight_alpha"),straight?1.f:0.f);
'@
Replace-Once $paths[3] "BlendScope state;`n`tMatrixScope matrix(params.x,params.y);" "BlendScope state(straight,params.legacyAlpha);`n`tMatrixScope matrix(params.x,params.y);"
Replace-Once $paths[3] "gs_effect_set_texture(image, texture);`n`twhile" "if(straight)gs_effect_set_texture_srgb(image,texture);else gs_effect_set_texture(image,texture);`n`twhile"
Replace-Once $paths[3] 'QImage ObsVisualRenderer::renderOffscreen(const visual::FramePtr &frame, float opacity, float offsetY)' @'
void ObsVisualRenderer::drawSolidRect(const QRectF &box,const QColor &color,bool active)
{
	if(!active)return;
	if(!box.isValid()||!std::isfinite(box.x())||!std::isfinite(box.y())||!std::isfinite(box.width())||!std::isfinite(box.height())||box.width()>2048||box.height()>2048||box.width()!=std::floor(box.width())||box.height()!=std::floor(box.height())||!color.isValid())throw std::invalid_argument("Invalid authored solid quad");
	if(!gs_get_context())throw std::logic_error("OBS graphics context required");
	auto *effect=obs_get_base_effect(OBS_EFFECT_SOLID);if(!effect)throw std::runtime_error("OBS solid effect unavailable");
	vec4 tint;vec4_set(&tint,float(color.redF()),float(color.greenF()),float(color.blueF()),float(color.alphaF()));
	gs_effect_set_vec4(gs_effect_get_param_by_name(effect,"color"),&tint);
	BlendScope blend(false,false,true);MatrixScope matrix(float(box.x()),float(box.y()));
	while(gs_effect_loop(effect,"Solid"))gs_draw_sprite(nullptr,0,uint32_t(box.width()),uint32_t(box.height()));
	++stats_.draws;
}
QImage ObsVisualRenderer::renderOffscreen(const visual::FramePtr &frame, float opacity, float offsetY)
'@
# Here strings keep backslashes literal; replace our indentation markers only.
foreach($path in $paths){$planned[$path]=$planned[$path].Replace('\t',"`t")}
New-Item -ItemType Directory -Path $folder -Force|Out-Null
$manifest=@()
foreach($path in $paths){
 $name=$path.Replace('/','__');$target=Join-Path $sharedRoot $path
 $before=Join-Path $folder ($name+'.before');$after=Join-Path $folder ($name+'.after')
 if(Test-Path -LiteralPath $before){
  if((Get-FileHash -LiteralPath $before).Hash -ne $expected[$path] -or [IO.File]::ReadAllText($after) -cne $planned[$path]){throw 'Preserve prior proposal; prepared source differs'}
 }else{[IO.File]::WriteAllBytes($before,[IO.File]::ReadAllBytes($target));[IO.File]::WriteAllText($after,$planned[$path],[Text.UTF8Encoding]::new($false))}
 $manifest+=[pscustomobject]@{Path=$path;Before=(Get-FileHash -LiteralPath $before).Hash;After=(Get-FileHash -LiteralPath $after).Hash}
}
$manifest|Export-Csv (Join-Path $folder 'manifest.csv') -NoTypeInformation
if(-not $Apply){Write-Output 'Prepared reversible four-file shared sRGB contract; no shared source modified';exit}
$lockBefore=@{};foreach($name in $locks){$lockBefore[$name]=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "visual-audit/$name"))}
try {
 foreach($row in $manifest){$target=Join-Path $sharedRoot $row.Path;if((Get-FileHash -LiteralPath $target).Hash -ne $row.Before){throw 'Source changed during preparation'};[IO.File]::WriteAllBytes($target,[IO.File]::ReadAllBytes((Join-Path $folder ($row.Path.Replace('/','__')+'.after'))))}
 foreach($name in $locks){$file=Join-Path $PSScriptRoot "visual-audit/$name";$rows=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json;foreach($row in $rows){$change=$manifest|Where-Object Path -eq $row.path;if($change){$row.sha256=$change.After}};[IO.File]::WriteAllText($file,($rows|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))}
 Write-Output 'Applied optional shared sRGB contract; originals backed up, input hashes updated'
}catch{
 foreach($name in $locks){[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot "visual-audit/$name"),$lockBefore[$name])}
 foreach($row in $manifest){$target=Join-Path $sharedRoot $row.Path;if((Get-FileHash -LiteralPath $target).Hash -eq $row.After){[IO.File]::WriteAllBytes($target,[IO.File]::ReadAllBytes((Join-Path $folder ($row.Path.Replace('/','__')+'.before'))))}}
 throw
}
