/* Explicit follow-up mapping; generated locators are not behavioral proof. */
"use strict";
const fs=require("node:fs"),path=require("node:path"),cp=require("node:child_process");
const root=path.resolve(__dirname,"../../..");
function entries(text) {
    const lines=text.split("\n"),out=[];
    for(let i=0;i<lines.length;i++) {
        if(!/^- \[[ x]\]/.test(lines[i]))continue;
        let end=i+1;
        while(end<lines.length&&!/^(?:- \[[ x]\]|## )/.test(lines[end]))end++;
        out.push({line:i+1,checked:lines[i].startsWith("- [x]"),title:lines[i].slice(6),text:lines.slice(i,end).join("\n").trim()});
    }
    return out;
}
const git=revision=>entries(cp.execFileSync("git",["show",revision+":TODO.md"],{cwd:root,encoding:"utf8"}));
/* Freeze the reviewed snapshot: later implemented milestones must not silently
 * change historical audit dispositions or break reproduction of the ledger. */
const before=git("172cd57"),current=git("15824f2");
const original=JSON.parse(fs.readFileSync(path.join(root,"artifacts/analysis/todo-audit-2026-11-04/claims.json"),"utf8"));
const inserted=before.findIndex(e=>e.title.startsWith("Reloadable image.decode module:"));
if(inserted<0 || current.length!==before.length || before.filter(e=>e.checked).length!==183)
    throw Error("Checklist changed: review the explicit follow-up mapping");
for(let i=0;i<before.length;i++) {
    if(i!==1 && i!==inserted && before[i].title!==current[i].title)
        throw Error("Heading changed or reordered at slot "+i+": review mapping");
    if(!before[i].checked && current[i].checked)
        throw Error("New checked item requires independent review at slot "+i);
}
const rows=[];
for(let slot=0;slot<before.length;slot++) {
    if(!before[slot].checked)continue;
    const old=slot===inserted?null:original.find(e=>e.slot===slot-(slot>inserted?1:0));
    if(slot!==inserted&&!old)throw Error("Missing previous checked claim at slot "+slot);
    const anchors=old?[...old.sources,...old.tests,...old.reports.map(p=>"artifacts/reports/"+p)]:[
        "openaurora/modules/image_decode/image_decode.c","openaurora/modules/ui_engine/ui_engine.c",
        "openaurora/tests/image_decode.c","openaurora/tests/ui_live_cards.c",
        "artifacts/reports/IMAGE-DECODE-2026-11-04.md"];
    for(const anchor of anchors)if(!fs.existsSync(path.join(root,anchor)))throw Error("Missing anchor "+anchor);
    rows.push({id:old?old.id:"C183",beforeLine:before[slot].line,currentLine:current[slot].line,
        title:before[slot].title,checked:current[slot].checked,
        disposition:!current[slot].checked?"reopened":old&&old.id==="C002"?"retained: alpha composition only; no scene transforms":"retained: bounded milestone; prior audit + unchanged source / fresh suite",
        anchors});
}
if(rows.length!==183 || rows.filter(r=>r.checked).length!==182 || rows.find(r=>!r.checked).id!=="C183")
    throw Error("Unexpected dispositions: review manually");
const md=["# Follow-up checked-claim ledger","",
    "All183 checks at172cd57. C001..C182 retain the prior audit IDs; C183 is the added PNG item.",
    "Locators are not proof. See TODO-REAUDIT-2026-11-04 for source review, tests and limits.","",
    "| ID | TODO line at15824f2 | Original heading | Disposition | Evidence anchors |",
    "|---|---|---|---|---|"];
for(const r of rows)md.push(`| ${r.id} | ${r.currentLine} | ${r.title.replaceAll("|","\\|")} | ${r.disposition} | ${r.anchors.join("; ")} |`);
fs.writeFileSync(path.join(__dirname,"checked-claims.md"),md.join("\n")+"\n");
console.log(JSON.stringify({checkedBefore:rows.length,checkedAtAudit:rows.filter(r=>r.checked).length,reopened:rows.filter(r=>!r.checked).map(r=>r.id)}));
