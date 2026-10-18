// Compare actual opaque GPU captures directly; no simulated compositing.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
async function main(){
 const [reference,candidate,report]=process.argv.slice(2);if(!report)throw Error('Reference, GPU captures and report required');
 const cases=JSON.parse(fs.readFileSync(path.join(reference,'glyph-reference-cases.json'))).cases;
 if(cases.length<12||cases.length>32)throw Error('Require complete bounded cases');
 const results=[];
 for(const item of cases)for(const matte of ['black','white']){
  const rectangle={left:0,top:0,width:item.width,height:item.height};
  const refFile=path.join(reference,`${item.id}-${matte}.png`);
  const marker=await sharp(refFile).extract({left:item.width-1,top:item.height+1,width:1,height:1}).ensureAlpha().raw().toBuffer();
  if(!marker.equals(Buffer.from([255,0,255,255])))throw Error('Missing CEF fonts-ready marker');
  const actualFile=path.join(candidate,`${item.id}-${matte}.png`),meta=await sharp(actualFile).metadata();
  if(meta.width!==item.width||meta.height!==item.height)throw Error('Wrong GPU dimensions');
  const a=await sharp(refFile).extract(rectangle).ensureAlpha().raw().toBuffer(),b=await sharp(actualFile).ensureAlpha().raw().toBuffer();
  let max=0,bad=0;
  for(let i=0;i<a.length;i+=4){let worst=0;for(let c=0;c<3;++c)worst=Math.max(worst,Math.abs(a[i+c]-b[i+c]));max=Math.max(max,worst);if(worst>2)++bad;if(a[i+3]!==255||b[i+3]!==255)throw Error('Opaque matte alpha changed');}
  results.push({id:item.id,matte,maxChannelError:max,pixelsOver2:bad,passes:max<=2});
 }
 const summary={scope:'Actual native OBS GPU versus actual OBS CEF opaque captures',tolerance:2,passed:results.filter(x=>x.passes).length,count:cases.length*2,maxChannelError:Math.max(...results.map(x=>x.maxChannelError)),results};
 fs.writeFileSync(report,JSON.stringify(summary,null,2)+'\n');console.log(JSON.stringify({...summary,results:undefined}));if(summary.passed!==summary.count)process.exitCode=1;
}
main().catch(error=>{console.error(error);process.exitCode=1});
