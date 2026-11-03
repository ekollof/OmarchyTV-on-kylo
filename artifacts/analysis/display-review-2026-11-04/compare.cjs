/* Compare only independent stock-reference widget regions, not whole-panel
 * parity (tab sets, values and focused typography remain different). */
"use strict";
const {execFileSync} = require("node:child_process");
const path = require("node:path");
function rgb(file, crop) {
    const args=[path.join(__dirname,file)];
    if(crop)args.push("-crop",crop,"+repage");
    return execFileSync("magick",[...args,"-depth","8","rgb:-"],{maxBuffer:8*1024*1024});
}
function compare(name,a,b,crop) {
    const aa=rgb(a,crop),bb=rgb(b,crop);
    if(aa.length!==bb.length)throw Error("different dimensions");
    let max=0,changed=0,over2=0,squared=0;
    for(let p=0;p<aa.length;p+=3) {
        let pixel=0;
        for(let c=0;c<3;c++) {
            const d=Math.abs(aa[p+c]-bb[p+c]);
            max=Math.max(max,d);pixel=Math.max(pixel,d);squared+=d*d;
        }
        if(pixel)changed++;if(pixel>2)over2++;
    }
    console.log(JSON.stringify({name,pixels:aa.length/3,changed,over2,max,rmse:Math.sqrt(squared/aa.length)}));
    if(over2)process.exitCode=1;
}
compare("focused Contrast slider","stock-contrast_0.png","focus1_1.png","165x49+489+274");
compare("Modus left chevron","stock-focus_1.png","focus0_1.png","18x24+399+208");
compare("Modus right chevron","stock-focus_1.png","focus0_1.png","18x24+743+208");
for(const phase of ["normal","focus0","focus1","focus2","focus3","focus4","focus7"])
    compare(`${phase} buffer pair`,`${phase}_0.png`,`${phase}_1.png`);
