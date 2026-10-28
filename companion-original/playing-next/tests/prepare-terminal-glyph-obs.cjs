// Test-only glyph pages. No renderer or browser dependency in the product.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'../..'),dest=path.resolve(process.argv[2]||'');
const allowed=path.join(root,'work')+path.sep;
if(!dest.toLowerCase().startsWith(allowed.toLowerCase())||!fs.statSync(dest).isDirectory())throw Error('Require existing owned directory under work');
const original=JSON.parse(fs.readFileSync(process.argv[3]?path.resolve(process.argv[3]):path.join(root,'outputs/phase12l-terminal-glyphs/reference.json')));
const rootClip=process.argv.includes('--unclipped-root')?'':'overflow:hidden;';
const cases=original.map(item=>{
 const width=Math.min(1024,item.width),height=item.height;
 const style={fontFamily:item.family,fontSize:item.pixelSize+'px',fontWeight:String(item.weight),letterSpacing:item.spacing+'px',color:item.color,lineHeight:item.cssLineHeight};
 if(item.italic)style.fontStyle='italic';
 if(item.padding)style.padding=item.padding+'px';
 if(item.textShadow)style.textShadow=item.textShadow;
 const state=JSON.stringify({text:item.text,style}).replace(/</g,'\\u003c');
 const html='<!doctype html><meta charset="utf-8"><style>html,body{margin:0;background:transparent;'+rootClip+'}#glyph{display:block;width:max-content;white-space:pre}#ready{position:absolute;right:0;bottom:0;width:1px;height:1px;background:#ff00ff;display:none}</style><div id="glyph"></div><div id="ready"></div><script>const state='+state+';const glyph=document.getElementById("glyph");glyph.textContent=state.text||"\\u00a0";Object.assign(glyph.style,state.style);if(!state.text)glyph.style.visibility="hidden";document.fonts.ready.then(()=>document.getElementById("ready").style.display="block");</script>';
 const file=path.join(dest,item.id+'.html');fs.writeFileSync(file,html);
 return {...item,width,height,captureHeight:height+2,fullWidth:item.width,localFile:file.replace(/\\/g,'/')};
});
fs.writeFileSync(path.join(dest,'glyph-reference-cases.json'),JSON.stringify({scope:'OBS CEF glyph-only diagnostic, first 1024 pixels; readiness marker outside crop',cases},null,2)+'\n');
