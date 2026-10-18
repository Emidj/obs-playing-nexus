// Diagnostic only: compare background alpha independently of differing metadata.
const fs=require('fs'),path=require('path'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..');
async function sample(file,x,y){const {data,info}=await sharp(path.join(root,file)).ensureAlpha().raw().toBuffer({resolveWithObject:true});const n=(y*info.width+x)*4;return {file,x,y,rgba:[...data.subarray(n,n+4)]};}
(async()=>{
 const original=await sample('outputs/phase12b-normal-32.2.2-32.2.2-references/studio-720-1.png',100,145);
 const native=await sample('outputs/phase12b-native-final-32.2.2-captures/native-prism.png',100,145);
 const result={scope:'Prism offset background only; no text comparison',original,native,alphaParity:original.rgba[3]===native.rgba[3],phase12Complete:false,
  finding:'Original solid quads use SRCALPHA/INVSRCALPHA; native common renderer uses premultiplied ONE/INVSRCALPHA. Opaque matte references are required before choosing compatibility behavior.'};
 fs.writeFileSync(path.join(root,'outputs/phase12b-alpha-diagnostic.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
})().catch(e=>{console.error(e);process.exitCode=1;});
