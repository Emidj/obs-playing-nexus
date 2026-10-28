// SPDX-License-Identifier: MIT
const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{
 const root=path.resolve(process.argv[2]);
 if(!root.toLowerCase().includes(`${path.sep}work${path.sep}`))throw Error('Fixtures require isolated work');
 await sharp({create:{width:128,height:128,channels:4,background:{r:255,g:0,b:0,alpha:1}}}).png().toFile(path.join(root,'fixture-red.png'));
 fs.writeFileSync(path.join(root,'fixture-blue.svg'),'<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128"><rect width="128" height="128" fill="#0000ff"/></svg>');
 fs.writeFileSync(path.join(root,'fixture-broken.png'),'not an image');
})();
