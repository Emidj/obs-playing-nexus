// GPU comparison of the actual development source with preserved original GDI.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..');
const [reference,capture,output]=process.argv.slice(2);
if(!reference||!capture||!output)throw Error('Require original directory, native directory and report');
(async()=>{
 const referenceInputs=JSON.parse(fs.readFileSync(path.resolve(root,reference,'normal-reference-metrics.json'),'utf8'));
 const nativeInputsPath=path.resolve(root,capture,'normal-matrix-inputs.json');
 const nativeInputs=fs.existsSync(nativeInputsPath)?JSON.parse(fs.readFileSync(nativeInputsPath,'utf8')):null;
 const metadataVerified=!!nativeInputs&&referenceInputs.every(r=>typeof r.title==='string'&&typeof r.artist==='string');
 if(process.argv.includes('--require-inputs')&&!metadataVerified)throw Error('Missing actual metadata evidence');
 if(metadataVerified){
  if(referenceInputs.length!==18||nativeInputs.length!==18)throw Error('Require eighteen metadata records');
  for(const r of referenceInputs){const n=nativeInputs.find(n=>n.name===r.name);if(!n||n.title!==r.title||n.artist!==r.artist)throw Error(`Metadata differs for ${r.name}`);}
 }
 const results=[];
 for(const preset of ['studio','minimal'])for(const width of [400,720,1600])for(const number of [1,2,3])for(const matte of ['black','white']) {
  const name=`${preset}-${width}-${number}-${matte}.png`;
  const a=await sharp(path.resolve(root,reference,name)).raw().toBuffer({resolveWithObject:true});
  const b=await sharp(path.resolve(root,capture,name)).raw().toBuffer({resolveWithObject:true});
  if(a.info.width!==b.info.width||a.info.height!==b.info.height||a.data.length!==b.data.length)throw Error(`Canvas mismatch ${name}`);
  let total=0,max=0,bad=0;
  for(let i=0;i<a.data.length;i++){const d=Math.abs(a.data[i]-b.data[i]);total+=d;max=Math.max(max,d);if(d>2)bad++;}
  results.push({name,meanChannelError:total/a.data.length,maxChannelError:max,channelsOver2:bad,passes:max<=2});
 }
 const report={schema:'playing-native-normal-cases-1',scope:'18 actual native/reference cases; opaque RGB black/white; initial scroll pause',metadataVerified,tolerance:2,allPass:results.every(r=>r.passes),results};
 fs.writeFileSync(path.resolve(root,output),JSON.stringify(report,null,2)+'\n');
 console.log(JSON.stringify({allPass:report.allPass,metadataVerified,cases:results.length,failed:results.filter(r=>!r.passes)}));
 if(process.argv.includes('--require-parity')&&!report.allPass)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1;});
