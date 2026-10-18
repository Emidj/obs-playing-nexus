// Independent RGBA gates against direct draws, not renderer counters.
const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
const [dir,out]=process.argv.slice(2);
async function load(file){const {data,info}=await sharp(path.join(dir,file)).ensureAlpha().raw().toBuffer({resolveWithObject:true});return {data,...info};}
function sum(image,box){let a=0,x=0,y=0;const r=box||[0,0,image.width,image.height];for(let py=r[1];py<r[3];py++)for(let px=r[0];px<r[2];px++){const alpha=image.data[(py*image.width+px)*4+3];a+=alpha;x+=px*alpha;y+=py*alpha;}return {alpha:a,x:a?x/a:0,y:a?y/a:0};}
function red(image){let alpha=0,x=0,y=0;for(let p=0;p<image.data.length;p+=4){const d=image.data;if(d[p]>245&&d[p+1]<5&&d[p+2]<5&&d[p+3]>10){const a=d[p+3],pixel=p/4;alpha+=a;x+=(pixel%image.width)*a;y+=Math.floor(pixel/image.width)*a;}}return {alpha,x:alpha?x/alpha:0,y:alpha?y/alpha:0};}
(async()=>{const checks=[];
for(const name of ['basic','prism','cantilever']){
 const images={};for(const sample of ['base','full','half','zero','slide','zoom'])images[sample]=await load(`${name}-transition-${sample}.png`);
 const base=images.base;const same=base.data.equals(images.full.data);
 checks.push({name,gate:'identity includes translucent edges',pass:same});
 checks.push({name,gate:'invisible midpoint clears all layers',pass:sum(images.zero).alpha===0});
 let maxAlphaError=0,spill=0;
 for(let p=0;p<base.data.length;p+=4){maxAlphaError=Math.max(maxAlphaError,Math.abs(images.half.data[p+3]-base.data[p+3]*.5));if(!base.data[p+3]&&images.half.data[p+3])spill++;}
 checks.push({name,gate:'whole fade alpha across text, decoration, image',maxAlphaError,spill,pass:maxAlphaError<=1&&spill===0});
 const artBox=[500,10,600,110];const art=sum(base,artBox),half=sum(images.half,artBox);
 checks.push({name,gate:'artwork fades once',ratio:half.alpha/art.alpha,pass:art.alpha>0&&Math.abs(half.alpha/art.alpha-.5)<.01});
 const slide=sum(images.slide);const zoom=sum(images.zoom);const original=sum(base);
 checks.push({name,gate:'slide moves complete presentation upward',deltaY:slide.y-original.y,pass:slide.alpha>0&&slide.y<original.y-5});
 checks.push({name,gate:'zoom shrinks complete presentation',ratio:zoom.alpha/original.alpha,pass:zoom.alpha>0&&zoom.alpha/original.alpha<.49&&zoom.alpha/original.alpha>.35});
 const redBase=red(base),redSlide=red(images.slide),redZoom=red(images.zoom);
 checks.push({name,gate:'artwork translates with text and decoration',deltaY:redSlide.y-redBase.y,pass:redBase.alpha>0&&redSlide.alpha>0&&redSlide.y<redBase.y-15&&Math.abs(redSlide.x-redBase.x)<1});
 const expectedX=base.width*.035+redBase.x*.93;
 checks.push({name,gate:'artwork scales around presentation centre',expectedX,actualX:redZoom.x,pass:redZoom.alpha>0&&Math.abs(redZoom.x-expectedX)<1&&redZoom.alpha/redBase.alpha<.45&&redZoom.alpha/redBase.alpha>.41});
 const live=await load(`${name}-live-final.png`);checks.push({name,gate:'real clock final presentation',pass:sum(live).alpha>0});
}
const result={pass:checks.every(c=>c.pass),checks};fs.writeFileSync(out,JSON.stringify(result,null,2));console.log(JSON.stringify(result));if(!result.pass)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1;});
