const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{const [dir,out]=process.argv.slice(2),cases=[];
 const files=fs.readdirSync(dir).filter(f=>f.endsWith('-transition-base.png'));
 if(files.length!==26)throw new Error('Require all 23 NORMAL and 3 aliases');
 for(const file of files){const id=file.replace('-transition-base.png','');const load=async name=>sharp(path.join(dir,`${id}-transition-${name}.png`)).ensureAlpha().raw().toBuffer();const base=await load('base'),full=await load('full'),half=await load('half');let error=0,ink=0,spill=0;
 for(let p=3;p<base.length;p+=4){if(base[p])ink++;error=Math.max(error,Math.abs(half[p]-base[p]*.5));if(!base[p]&&half[p])spill++;}
 cases.push({id,identity:base.equals(full),maxAlphaError:error,spill,ink,pass:base.equals(full)&&error<=1&&spill===0&&ink>0});}
 const result={pass:cases.every(c=>c.pass),cases};fs.writeFileSync(out,JSON.stringify(result,null,2));console.log(JSON.stringify({pass:result.pass,cases:cases.length,failed:cases.filter(c=>!c.pass)}));if(!result.pass)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});
