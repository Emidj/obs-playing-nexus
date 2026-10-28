// Test-only fixture, preserving the original Terminal title shadow.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'../..'),output=path.resolve(process.argv[2]||'');
if(!output.startsWith(path.join(root,'outputs')+path.sep)||fs.existsSync(output))throw Error('New fixture under outputs required');
const cases=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12r-terminal-five-texts/reference.json')));
for(const item of cases){item.padding=12;item.width+=24;item.height+=24;
 if(item.row===1){item.textShadow='rgba(99, 233, 142, 0.266667) 0px 0px 7px';item.shadowRadius=7;item.shadowColor='#4463e98e';}
}
// QColor string is #AARRGGBB; original CSS #63e98e44 is #RRGGBBAA.
fs.writeFileSync(output,JSON.stringify(cases,null,2)+'\n');
