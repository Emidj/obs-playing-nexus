// Glyph-only fixture. Styles/text come from the preserved 20-design DOM bank.
const fs=require('fs'),path=require('path'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../..'),destination=path.join(root,'outputs/phase12w-normal-glyphs');
async function main(){
 const source=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12v-other-normal-text/reference.json')));
 fs.mkdirSync(destination,{recursive:true});
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 try{
  const page=await browser.newPage({viewport:{width:1600,height:256}});
  const cdp=await page.context().newCDPSession(page);await cdp.send('DOM.enable');await cdp.send('CSS.enable');
  for(const kind of ['short','unicode']){const cases=[];
   for(const original of source.cases.filter(item=>item.kind===kind)){
    const row=original.rows[1];if(!Number.isInteger(row.pixelSize))throw Error('Fractional native font size requires explicit support');
    await page.setContent('<style>html,body{margin:0}#glyph{display:block;width:max-content;white-space:pre}</style><div id="glyph"></div>');
    await page.evaluate(row=>{const glyph=document.getElementById('glyph');glyph.textContent=row.transform==='uppercase'?row.text.toUpperCase():row.text;
     Object.assign(glyph.style,{fontFamily:row.family,fontSize:row.pixelSize+'px',fontWeight:row.weight,fontStyle:row.fontStyle,letterSpacing:row.spacing,lineHeight:row.lineHeight,color:row.color});},row);
    await page.evaluate(()=>document.fonts.ready);
    const metrics=await page.evaluate(()=>{const glyph=document.getElementById('glyph'),rect=glyph.getBoundingClientRect();const marker=document.createElement('span');marker.style.cssText='display:inline-block;width:0;height:0;vertical-align:baseline';glyph.append(marker);const baseline=marker.getBoundingClientRect().y-rect.y;marker.remove();return {text:glyph.textContent,advance:rect.width,lineHeight:rect.height,baseline};});
    const document=await cdp.send('DOM.getDocument');const node=await cdp.send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#glyph'});
    metrics.platformFonts=(await cdp.send('CSS.getPlatformFontsForNode',{nodeId:node.nodeId})).fonts;
    cases.push({id:original.preset+'-'+kind,family:row.family,pixelSize:row.pixelSize,weight:Number(row.weight),italic:row.fontStyle==='italic',spacing:parseFloat(row.spacing)||0,color:row.color,cssLineHeight:row.lineHeight,padding:12,width:Math.ceil(metrics.advance)+26,height:Math.ceil(metrics.lineHeight)+24,...metrics});
   }
   if(cases.length!==20)throw Error('Incomplete original catalogue');
   fs.writeFileSync(path.join(destination,kind+'.json'),JSON.stringify(cases,null,2)+'\n');
  }
 }finally{await browser.close()}
 fs.writeFileSync(path.join(destination,'provenance.json'),JSON.stringify({scope:'Original title font and casing only; no scene/shadow parity',browser:browser.version(),inputs:source.inputs},null,2)+'\n');
 console.log('40 original title glyph fixtures, 20 designs, short/Unicode');
}
main().catch(error=>{console.error(error);process.exitCode=1});
