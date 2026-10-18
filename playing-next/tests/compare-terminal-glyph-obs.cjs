// OBS mattes avoid interpreting a framebuffer's blend alpha as glyph alpha.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..'),folder=path.resolve(process.argv[2]||'');
const bank=path.join(root,'outputs/phase12l-terminal-glyphs');
async function raw(file,rectangle){return sharp(file).extract(rectangle).ensureAlpha().raw().toBuffer()}
const decode=s=>s<=.04045?s/12.92:Math.pow((s+.055)/1.055,2.4);
const encode=l=>l<=.0031308?12.92*l:1.055*Math.pow(l,1/2.4)-.055;
async function main(){
 const cases=JSON.parse(fs.readFileSync(path.join(folder,'glyph-reference-cases.json'))).cases;
 if(cases.length!==12)throw Error('Require 12 CEF cases');
 const candidates=['chrome155','qt-sdk','qt-obs3222','dwrite-sdk','dwrite-downsample','skia-h2-s1'];
 const extra=path.join(root,'outputs/phase12m-dwrite-natural');
 if(fs.existsSync(path.join(extra,'short-0-native.png')))candidates.push('dwrite-natural');
 const supplied=new Map();for(const arg of process.argv.filter(x=>x.startsWith('--candidate='))){
  const value=arg.slice('--candidate='.length),separator=value.indexOf('=');
  if(separator<1)throw Error('Candidate requires name=directory');
  const name=value.slice(0,separator);if(candidates.includes(name)||supplied.has(name))throw Error('Duplicate candidate name');
  supplied.set(name,path.resolve(value.slice(separator+1)));candidates.push(name);
 }
 const results=[];
 for(const item of cases){
  const rect={left:0,top:0,width:item.width,height:item.height},mattes={};
  for(const matte of ['transparent','black','white']){
   const file=path.join(folder,item.id+'-'+matte+'.png');
   const metadata=await sharp(file).metadata();
   if(metadata.width!==item.width||metadata.height!==item.captureHeight)throw Error('Wrong CEF capture dimensions');
   const marker=await raw(file,{left:item.width-1,top:item.height+1,width:1,height:1});
   if(marker[0]!==255||marker[1]!==0||marker[2]!==255||marker[3]!==255)throw Error('CEF fonts-ready marker missing: '+item.id+' '+matte);
   mattes[matte]=await raw(file,rect);
  }
  for(const candidate of candidates){
   const file=supplied.has(candidate)?path.join(supplied.get(candidate),item.id+'-native.png'):candidate==='chrome155'?path.join(bank,item.id+'-browser.png'):candidate==='dwrite-natural'?path.join(extra,item.id+'-native.png'):path.join(bank,candidate,item.id+'-native.png');
   const pixels=await raw(file,rect);let max=0,bad=0,total=0;
   for(const matte of ['black','white']){const background=matte==='white'?255:0,actual=mattes[matte];
    for(let i=0;i<pixels.length;i+=4){let worst=0;
     // OBS browser uses ONE/INVSRCALPHA with an sRGB sampled texture and
     // sRGB framebuffer. Decode the premultiplied stored byte, not straight
     // RGB times alpha. This differs from an encoded RGB source-over on white.
     for(let c=0;c<3;++c){const premul=Math.round(pixels[i+c]*pixels[i+3]/255)/255;
      const expected=255*encode(Math.min(1,decode(premul)+background/255*(1-pixels[i+3]/255)));
      const delta=Math.abs(expected-actual[i+c]);max=Math.max(max,delta);worst=Math.max(worst,delta);total+=delta;}
     if(actual[i+3]!==255)throw Error('Opaque matte is not opaque');if(worst>2)bad++;
    }
   }
   results.push({id:item.id,candidate,passes:max<=2,maxChannelError:max,pixelsOver2:bad,meanChannelError:total/(pixels.length/4*6)});
  }
 }
 const summary=candidates.map(candidate=>{const rows=results.filter(x=>x.candidate===candidate);return {candidate,passes:rows.every(x=>x.passes),passed:rows.filter(x=>x.passes).length,count:rows.length,maxChannelError:Math.max(...rows.map(x=>x.maxChannelError))}});
 const report={scope:'OBS CEF font-only raster, first 1024 pixels; sRGB sampled premultiplied bytes / ONE blend / sRGB framebuffer expected; native GPU scene not certified',tolerance:2,readyMarkers:36,summary,results};
 const destination=process.argv.find(x=>x.startsWith('--report='));
 fs.writeFileSync(destination?path.resolve(destination.slice('--report='.length)):path.join(folder,'comparison.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(summary));
 const required=process.argv.find(x=>x.startsWith('--require-candidate='));
 if(required){const name=required.slice('--require-candidate='.length),row=summary.find(x=>x.candidate===name);
  if(!row)throw Error('Unknown required candidate');if(!row.passes)process.exitCode=1;
 }
}
main().catch(error=>{console.error(error);process.exitCode=1});
