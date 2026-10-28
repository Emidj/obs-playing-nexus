// Compare premultiplied colors: RGB in fully transparent pixels has no visual
// meaning. Also compare alpha so a missing edge cannot be hidden by this rule.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..'),folder=path.join(root,'work/phase12-native-planes');
async function main(){
 const candidates=JSON.parse(fs.readFileSync(path.join(folder,'result.json'),'utf8'));
 if(candidates.length!==21)throw Error('Expected all 21 DOM originals');
 const results=[];
 for(const preset of candidates){
  const samples=[];
  for(const time of [0,1500,3000]){
   const a=await sharp(path.join(folder,`${preset.id}-background-${time}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
   const b=await sharp(path.join(folder,`${preset.id}-composed-${time}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
   if(a.info.width!==b.info.width||a.info.height!==b.info.height)throw Error('Canvas mismatch');
   let sum=0,max=0,bad=0;
   for(let i=0;i<a.data.length;i+=4){let pixelMax=0;
    for(let c=0;c<4;c++){
     const delta=Math.abs(c===3?a.data[i+c]-b.data[i+c]:a.data[i+c]*a.data[i+3]/255-b.data[i+c]*b.data[i+3]/255);
     sum+=delta;max=Math.max(max,delta);pixelMax=Math.max(pixelMax,delta);
    }
    if(pixelMax>2)bad++;
   }
   samples.push({time,meanPremultipliedChannelError:sum/a.data.length,maxChannelError:max,pixelsOver2:bad,totalPixels:a.info.width*a.info.height});
  }
  results.push({id:preset.id,passes:samples.every(s=>s.maxChannelError<=2),samples});
 }
 const report={schema:'playing-normal-plane-parity-1',scope:'Candidate background decomposition only; text and native OBS not tested',tolerance:2,allPass:results.every(p=>p.passes),results};
 const destination=path.join(root,'outputs/phase12-reference/plane-parity.json');
 fs.writeFileSync(destination,JSON.stringify(report,null,2)+'\n');
 console.log(`${results.filter(p=>p.passes).length}/21 candidate backgrounds pass; native integration is pending`);
 if(process.argv.includes('--require-parity')&&!report.allPass)process.exitCode=1;
}
main().catch(e=>{console.error(e);process.exitCode=1;});
