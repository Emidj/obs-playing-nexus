const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{
 const [dir,out]=process.argv.slice(2),images={};
 for(const name of ['static-a','static-b','vinyl-a','vinyl-b','changed','static-final']){
  const {data,info}=await sharp(path.join(dir,`optimization-${name}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(!data.some((v,i)=>i%4===3&&v>0))throw Error(`Empty ${name}`);images[name]={data,info};
 }
 const equal=(a,b)=>images[a].info.width===images[b].info.width&&images[a].info.height===images[b].info.height&&images[a].data.equals(images[b].data);
 const checks=[{gate:'Static RGBA unchanged while vinyl animates',pass:equal('static-a','static-b')},{gate:'Vinyl continues moving',pass:!equal('vinyl-a','vinyl-b')},{gate:'Metadata change updates static source',pass:!equal('static-a','changed')},{gate:'Static final RGBA unchanged after animated consumer hidden',pass:equal('changed','static-final')}];
 fs.writeFileSync(out,JSON.stringify({checks,pass:checks.every(c=>c.pass)},null,2)+'\n');
 for(const c of checks)console.log(`${c.pass?'PASS':'FAIL'} ${c.gate}`);if(checks.some(c=>!c.pass))process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});
