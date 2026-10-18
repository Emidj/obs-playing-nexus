const fs=require('fs'),path=require('path');
const [root,out]=process.argv.slice(2);
if(!root||!out)throw new Error('Require completed profiling directory and output JSON');
const rows=JSON.parse(fs.readFileSync(path.join(root,'samples.json'),'utf8').replace(/^\uFEFF/,''));
const mean=(a,k)=>a.reduce((n,r)=>n+Number(r[k]),0)/a.length;
const range=(a,k)=>({first:Number(a[0][k]),last:Number(a.at(-1)[k]),min:Math.min(...a.map(r=>Number(r[k]))),max:Math.max(...a.map(r=>Number(r[k])))});
const cases=[];
for(const name of [...new Set(rows.map(r=>r.Scenario))]){
 const a=rows.filter(r=>r.Scenario===name);
 const weight=a.reduce((n,r)=>n+Number(r.SampleIntervalSeconds||1),0);
 const cpu=a.reduce((n,r)=>n+Number(r.CpuMachinePercent)*Number(r.SampleIntervalSeconds||1),0)/weight;
 const chunks=[];const start=Number(a[0].ElapsedSeconds);
 for(let minute=0;minute<=Math.floor((Number(a.at(-1).ElapsedSeconds)-start)/300);minute++){
  const b=a.filter(r=>Math.floor((Number(r.ElapsedSeconds)-start)/300)===minute);if(!b.length)continue;
  chunks.push({fromMinute:minute*5,samples:b.length,privateMiB:range(b,'PrivateMiB'),handles:range(b,'Handles'),threads:range(b,'Threads')});
 }
 cases.push({name,samples:a.length,observedSeconds:Number(a.at(-1).VideoElapsedSeconds)-Number(a[0].VideoElapsedSeconds),cpuMachineWeightedMean:cpu,
  gpuEngineMean:mean(a,'GpuEngineMaxPercent'),privateMiB:range(a,'PrivateMiB'),workingMiB:range(a,'WorkingMiB'),handles:range(a,'Handles'),threads:range(a,'Threads'),
  rawFps:(Number(a.at(-1).OutputFrames)-Number(a[0].OutputFrames))/(Number(a.at(-1).VideoElapsedSeconds)-Number(a[0].VideoElapsedSeconds)),
  laggedFrameDelta:Number(a.at(-1).LaggedFrames)-Number(a[0].LaggedFrames),chunks});
}
const result={scope:'One owned OBS process; single local session; source requests change every two seconds; individual songs not visually audited',environment:JSON.parse(fs.readFileSync(path.join(root,'environment.json'),'utf8').replace(/^\uFEFF/,'')),cases};
fs.writeFileSync(out,JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(cases.map(c=>({name:c.name,samples:c.samples,observedSeconds:c.observedSeconds,rawFps:c.rawFps,privateMiB:c.privateMiB,laggedFrameDelta:c.laggedFrameDelta}))));
