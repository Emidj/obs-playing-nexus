// Validate real GPU captures; decorations cannot satisfy the text-scroll gate.
const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{
 const dir=process.argv[2],out=process.argv[3];
 if(!dir||!out)throw new Error('Require native gallery and report path');
 const catalogue=JSON.parse(fs.readFileSync(path.join(__dirname,'../../outputs/phase12-reference/catalogue.json')));
 const names=[...catalogue.catalogue.map(x=>x.id),...Object.keys(catalogue.aliases)];
 const result={scope:'Actual shared renderer GPU: 23 NORMAL and 3 aliases',cases:[],pass:true};
 for(const id of names){
  const capture=async(suffix)=>{
   const file=path.join(dir,id+'-'+suffix+'.png');const {data,info}=await sharp(file).ensureAlpha().raw().toBuffer({resolveWithObject:true});
   let ink=0;for(let i=3;i<data.length;i+=4)if(data[i]>0)ink++;
   if(info.width!==720||info.height>2048||!ink)throw new Error('Invalid GPU capture '+id+' '+suffix);
   return {data,info,ink};
  };
  const short=await capture('short'),motion=await capture('motion'),unicode=await capture('unicode'),long=await capture('long');
  const a=await capture('text-a'),b=await capture('text-b');
  const changes=(x,y)=>{if(x.data.length!==y.data.length)return -1;let n=0;for(let i=0;i<x.data.length;i+=4)if(x.data[i]!==y.data[i]||x.data[i+1]!==y.data[i+1]||x.data[i+2]!==y.data[i+2]||x.data[i+3]!==y.data[i+3])n++;return n;};
  const textChanged=changes(a,b),decorChanged=changes(short,motion);
  const animated=!['studio','minimal'].includes(id);
  const pass=textChanged>100&&(!animated||decorChanged>5);
  result.cases.push({id,canvas:[short.info.width,short.info.height],unicodePixels:unicode.ink,longPixels:long.ink,textScrollChangedPixels:textChanged,decorationChangedPixels:decorChanged,pass});
  result.pass&&=pass;
 }
 fs.writeFileSync(out,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({pass:result.pass,cases:result.cases.length,failed:result.cases.filter(x=>!x.pass)}));
 if(!result.pass)process.exitCode=1;
})().catch(e=>{console.error(e.message);process.exitCode=1});
