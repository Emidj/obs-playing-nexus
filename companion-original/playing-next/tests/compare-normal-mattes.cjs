// Deterministic opaque RGB comparison. Neither fixture uses runtime Browser.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..');
const reference=process.argv[2]||'outputs/phase12c-reference-32.2.2-references';
const capture=process.argv[3]||'outputs/phase12c-native-32.2.2-captures';
const output=process.argv[4]||'outputs/phase12c-matte-parity.json';
(async()=>{
 const results=[];
 for(const [original,native] of [['studio','prism'],['minimal','air']])for(const matte of ['black','white']) {
  const a=await sharp(path.resolve(root,reference,`${original}-720-1-${matte}.png`)).raw().toBuffer({resolveWithObject:true});
  const b=await sharp(path.resolve(root,capture,`native-${native}-${matte}.png`)).raw().toBuffer({resolveWithObject:true});
  if(a.info.width!==b.info.width||a.info.height!==b.info.height||a.data.length!==b.data.length)throw new Error('Reference/native canvas mismatch');
  let total=0,max=0,bad=0;
  for(let i=0;i<a.data.length;i++){const delta=Math.abs(a.data[i]-b.data[i]);total+=delta;max=Math.max(max,delta);if(delta>2)bad++;}
  results.push({preset:original,matte,meanChannelError:total/a.data.length,maxChannelError:max,channelsOver2:bad,passes:max<=2});
 }
 const result={schema:'playing-normal-matte-parity-1',scope:'Prism/Air, width720, short matching title/artist, exact original t0, framebuffer RGB on opaque mattes',tolerance:2,allPass:results.every(r=>r.passes),results};
 fs.writeFileSync(path.resolve(root,output),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
 // It is an audit, not a passing test: output carries the explicit failed gate.
})().catch(e=>{console.error(e);process.exitCode=1;});
