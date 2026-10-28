const fs=require('fs'),path=require('path'),sharp=require('sharp');
const supplied=process.argv.slice(2).filter(x=>!x.startsWith('--'));
const folder=supplied[0]?path.resolve(supplied[0]):path.resolve(__dirname,'../../outputs/phase12l-terminal-glyphs');
const nativeFolder=supplied[1]?path.resolve(supplied[1]):folder;
async function main(){const cases=JSON.parse(fs.readFileSync(path.join(folder,'reference.json'))),results=[];
 for(const item of cases){const a=await sharp(path.join(folder,item.id+'-browser.png')).ensureAlpha().raw().toBuffer({resolveWithObject:true});const b=await sharp(path.join(nativeFolder,item.id+'-native.png')).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(a.info.width!==b.info.width||a.info.height!==b.info.height)throw Error('Dimensions differ');let max=0,bad=0,sum=0;
  for(let i=0;i<a.data.length;i+=4){let pixel=0;for(let c=0;c<4;++c){const delta=Math.abs(c===3?a.data[i+c]-b.data[i+c]:a.data[i+c]*a.data[i+3]/255-b.data[i+c]*b.data[i+3]/255);max=Math.max(max,delta);sum+=delta;pixel=Math.max(pixel,delta)}if(pixel>2)bad++}
  results.push({id:item.id,passes:max<=2,maxChannelError:max,pixelsOver2:bad,meanChannelError:sum/a.data.length});
 }
 const report={scope:'Glyph raster only; shadows and scene excluded',tolerance:2,allPass:results.every(x=>x.passes),results};fs.writeFileSync(path.join(nativeFolder,'comparison.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));if(process.argv.includes('--require-parity')&&!report.allPass)process.exitCode=1;
}
main().catch(e=>{console.error(e);process.exitCode=1});
