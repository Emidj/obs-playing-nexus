// Independent glyph raster reference; CSS shadows and scene composition excluded.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const {pathToFileURL}=require('url'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../..'),dest=process.argv[2]?path.resolve(process.argv[2]):path.join(root,'outputs/phase12l-terminal-glyphs');
const includeStatic=process.argv.includes('--include-static');
async function main(){
 fs.mkdirSync(dest,{recursive:true});const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const cases=[];
 try{
  const page=await browser.newPage({viewport:{width:400,height:500}});
  await page.addInitScript(()=>window.requestAnimationFrame=()=>0);
  await page.goto(pathToFileURL(path.join(root,'obs-playing/designs.html')).href);
  const cdp=await page.context().newCDPSession(page);await cdp.send('DOM.enable');await cdp.send('CSS.enable');
  const capture=await browser.newPage({viewport:{width:8192,height:256}});
  for(const [id,title,artist] of [['short','Lipstick','Alaia & Gallo'],['unicode','Jóga 🎵 日本語','Björk'],['long','Long track '.repeat(40),'Long artist '.repeat(30)],['empty','','']]){
   await page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),{preset:'terminal',width:400,gap:12,original:true,title,artist,label:'NOW PLAYING '+id,visible:true,opacity:1,speed:40,pause:2,transition:'fade',duration:800,restart:0});
   await page.evaluate(()=>document.fonts.ready);
   const rows=await page.evaluate(includeStatic=>[...document.querySelectorAll('#foreground .run'),...(includeStatic?[document.querySelector('#foreground .terminal-top>span:last-child'),document.querySelector('#foreground .terminal-prompt')]:[])].map(run=>{
    const s=getComputedStyle(run),rect=run.getBoundingClientRect();
    const marker=document.createElement('span');marker.style.cssText='display:inline-block;width:0;height:0;vertical-align:baseline';run.append(marker);
    const baseline=marker.getBoundingClientRect().y-rect.y;marker.remove();
    return {text:run.textContent,family:s.fontFamily,pixelSize:parseFloat(s.fontSize),weight:Number(s.fontWeight),spacing:parseFloat(s.letterSpacing)||0,color:s.color,lineHeight:rect.height,baseline,advance:rect.width,cssLineHeight:s.lineHeight};
   }),includeStatic&&id==='short');
   const document=await cdp.send('DOM.getDocument');
   for(let row=0;row<rows.length;++row){
    const selector=row<3?'#foreground .run':row===3?'#foreground .terminal-top>span:last-child':'#foreground .terminal-prompt';
    const nodes=await cdp.send('DOM.querySelectorAll',{nodeId:document.root.nodeId,selector});
    rows[row].platformFonts=(await cdp.send('CSS.getPlatformFontsForNode',{nodeId:nodes.nodeIds[row<3?row:0]})).fonts;
   }
   for(let row=0;row<rows.length;++row){const item={id:id+'-'+row,row,...rows[row]};
    item.width=Math.ceil(item.advance)+2;item.height=Math.ceil(item.lineHeight);
    await capture.setContent('<style>html,body{margin:0;background:transparent}#glyph{display:block;width:max-content;white-space:pre}</style><div id="glyph"></div>');
    await capture.evaluate(item=>{const n=document.getElementById('glyph');n.textContent=item.text||'\u00a0';Object.assign(n.style,{fontFamily:item.family,fontSize:item.pixelSize+'px',fontWeight:String(item.weight),letterSpacing:item.spacing+'px',color:item.color,lineHeight:item.cssLineHeight});if(!item.text)n.style.visibility='hidden'},item);
    await capture.evaluate(()=>document.fonts.ready);
    await capture.screenshot({path:path.join(dest,item.id+'-browser.png'),omitBackground:true,clip:{x:0,y:0,width:item.width,height:item.height}});
    cases.push(item);
   }
  }
 }finally{await browser.close()}
 fs.writeFileSync(path.join(dest,'reference.json'),JSON.stringify(cases,null,2)+'\n');
 const inputs=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12e-dom-contracts/terminal.json'))).inputs.map(({file})=>({file,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'obs-playing',file))).digest('hex')}));
 fs.writeFileSync(path.join(dest,'provenance.json'),JSON.stringify({scope:'Terminal glyphs only; no text shadow, scene or native OBS parity',browser:browser.version(),inputs},null,2)+'\n');
 console.log(cases.length+' original glyph strips');
}
main().catch(e=>{console.error(e);process.exitCode=1});
