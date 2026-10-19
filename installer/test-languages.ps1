param([Parameter(Mandatory=$true)][string]$Installer,[Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$root=[IO.Path]::GetFullPath((Join-Path (Resolve-Path "$PSScriptRoot/../..") $OutputRoot))
$allowed=[IO.Path]::GetFullPath((Join-Path "$PSScriptRoot/../.." 'work'))+'\'
if(-not $root.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $root)){throw 'New output under work required'}
New-Item -ItemType Directory -Path $root|Out-Null
$assembly=[Reflection.Assembly]::LoadFrom((Resolve-Path $Installer).Path)
$locale=$assembly.GetType('L');$wizard=$assembly.GetType('Wizard');$flags=[Reflection.BindingFlags]'NonPublic,Static'
$set=$locale.GetMethod('SetLanguage',$flags);$translate=$locale.GetMethod('T',$flags)
$checks=0
function Check($condition,$why){if(-not $condition){throw $why};$script:checks++}
$codes=@('es-ES','en-US','it-IT','fr-FR','de-DE');$expected=@('Instalar','Install','Installa','Installer','Installieren')
for($i=0;$i -lt 5;$i++){
 $set.Invoke($null,@($codes[$i]))|Out-Null
 Check ($translate.Invoke($null,@('Instalar')) -eq $expected[$i]) 'Independent installer language expectation'
 $form=$wizard.GetConstructor([Reflection.BindingFlags]'Instance,NonPublic',$null,[Type[]]@(),$null).Invoke(@())
 try{
  $form.ShowInTaskbar=$false;$form.Show();[Windows.Forms.Application]::DoEvents();$form.PerformLayout()
  $buttons=@($form.Controls|Where-Object {$_ -is [Windows.Forms.Button]})
  Check (@($buttons|Where-Object Text -eq $expected[$i]).Count -eq 1) 'Install button not translated'
  foreach($button in $buttons){$measured=[Windows.Forms.TextRenderer]::MeasureText($button.Text,$button.Font,[Drawing.Size]::new(10000,10000),[Windows.Forms.TextFormatFlags]::SingleLine);Check ($measured.Width -le $button.ClientSize.Width-6) 'Translated button text clipped'}
  foreach($control in $form.Controls){Check ($form.ClientRectangle.Contains($control.Bounds)) 'Translated installer control outside window'}
  $bitmap=New-Object Drawing.Bitmap($form.Width,$form.Height)
  try{$form.DrawToBitmap($bitmap,[Drawing.Rectangle]::new(0,0,$form.Width,$form.Height));$bitmap.Save((Join-Path $root ($codes[$i]+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
 }finally{$form.Close();$form.Dispose()}
}
$set.Invoke($null,@('pt-BR'))|Out-Null
Check ($translate.Invoke($null,@('Instalar')) -eq 'Install') 'Unsupported installer language does not fall back to English'
Write-Output "PASS installer five-language checks=$checks; rendered all five forms; no installation or registry writes"
