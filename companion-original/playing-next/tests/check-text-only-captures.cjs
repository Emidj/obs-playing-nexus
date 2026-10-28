const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{const [dir,out]=process.argv.slice(2),cases=[];for(const name of ['horizontal-off','horizontal-on','vertical-off','vertical-on','custom']) {
 const {data,info}=await sharp(path.join(dir,`textonly-${name}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});let red=0,cyan=0,white=0,yellow=0,ink=0;
 for(let p=0;p<data.length;p+=4){if(!data[p+3])continue;ink++;const [r,g,b]=data.subarray(p,p+3);if(r>245&&g<5&&b<5)red++;if(r<5&&g>245&&b>245)cyan++;if(r>245&&g>245&&b>245)white++;if(r>245&&g>245&&b<5)yellow++;}
 const vertical=name.startsWith('vertical'),cover=name.endsWith('-on');const pass=info.width===(vertical?320:640)&&info.height===(vertical?360:120)&&ink>300&&ink<info.width*info.height/2&&cyan>30&&(name==='custom'?yellow>100:white>100)&&(cover?red===6400:red===0);
 cases.push({name,width:info.width,height:info.height,ink,red,cyan,white,yellow,pass});}
 const result={pass:cases.every(c=>c.pass),cases};fs.writeFileSync(out,JSON.stringify(result,null,2));console.log(JSON.stringify(result));if(!result.pass)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});
