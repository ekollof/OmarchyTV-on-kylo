/* Read-only locators for the pre-audit checked claims. This does not certify
 * implementations: source/test/evidence review is recorded in the report. */
"use strict";
const fs=require("node:fs"),path=require("node:path"),cp=require("node:child_process");
const root=path.resolve(__dirname,"../../..");
const original=cp.execFileSync("git",["show","432c71f:TODO.md"],{cwd:root,encoding:"utf8"});
const reportDir=path.join(root,"artifacts/reports");
const reports=fs.readdirSync(reportDir).filter(n=>n.endsWith(".md") && n!=="README.md");
const lines=original.split("\n"),entries=[];
let section="",slot=-1;
for(let at=0;at<lines.length;at++) {
    if(lines[at].startsWith("## "))section=lines[at].slice(3);
    if(/^- \[[ x]\]/.test(lines[at]))slot++;
    if(!lines[at].startsWith("- [x]"))continue;
    let end=at+1;
    while(end<lines.length && !/^(?:- \[[ x]\]|## )/.test(lines[end]))end++;
    entries.push({id:`C${String(entries.length+1).padStart(3,"0")}`,slot,line:at+1,
        section,title:lines[at].slice(6),text:lines.slice(at,end).join("\n").trim()});
}
const currentLines=fs.readFileSync(path.join(root,"TODO.md"),"utf8").split("\n"),current=[];
for(let at=0;at<currentLines.length;at++) {
    if(!/^- \[[ x]\]/.test(currentLines[at]))continue;
    let end=at+1;
    while(end<currentLines.length&&!/^(?:- \[[ x]\]|## )/.test(currentLines[end]))end++;
    current.push({line:at+1,checked:currentLines[at].startsWith("- [x]"),
        text:currentLines.slice(at,end).join("\n").trim()});
}
if(current.length!==slot+1)throw Error("checklist reordered/extended: update audit slot mapping explicitly");
for(const e of entries) {
    e.current=current[e.slot];
    e.disposition=!e.current.checked?"reopened":e.text===e.current.text?"retained scoped milestone":"retained; wording corrected/narrowed";
}
const extra=[
    [/Offline (?:display|layout)/,"UI-FB-SIM"],
    [/Palette|KER_OSD|Pool construction|generic GEM/,"OSD-BUFFER-CONTRACT"],
    [/Every present|Alpha callback|Update callback/,"OSD-KERNEL-INIT"],
    [/fbregion/,"OSD-MAU-SELECTION"],
    [/PWM six/,"PWM-CONTRACT-RECHECK"],
    [/Socket\/stdin|Host recovery/,"MODULE-SAFETY"],
    [/UI engine first slice/,"UI-ENGINE"],
    [/Skin seam/,"UI-SKINS"],
    [/Tile hit|Portal rail/,"UI-PARITY"],
    [/Source-selection/,"UI-SOURCES"],
    [/Captured picture-settings/,"SETTINGS"],
    [/Captured full picture/,"SETTINGS-PANEL"],
    [/Picture-settings row selection/,"SETTINGS-FOCUS"],
    [/Lab slider/,"SETTINGS-EDIT"],
    [/Harden ui_vendor/,"UI-WORK-REVIEW"],
    [/154 navigation/,"NAVIGATION-CASE-AUDIT"],
    [/Recovery succeeded/,"STABILITY"],
    [/workbench/,"WORKBENCH-MIGRATION"],
];
/* Manually inspected anchors where a dated report uses only a basename or
 * the claim is historical/research rather than an implementation. */
const manual={
 C003:["common/svg_scrollbar.c","tests/ui_live_cards.c"],
 C004:["modules/ui_engine/ui_engine.c","tests/ui_async.c"],
 C005:["core/module.c","tests/quiesce.c"],
 C007:["common/audio_owner.c","common/cec_owner.c","tests/ui_anim_review.c"],
 C008:["modules/ui_vendor/ui_vendor.c","tests/ui_rebind.c"],
 C009:["tools/audio/oa-audio-broker.c","tests/audio_broker.c"],
 C010:["modules/audio_mixer/audio_mixer.c","tests/mixer_module.c"],
 C017:["core/control.c","tests/control_reply.c"],
 C019:["core/check.c","tests/runtime_check.sh"],
 C022:["core/module.c","tests/phases.c"],
 C023:["core/host.c","tests/power_cycle.c"],
 C024:["modules/power/power.c","tests/power_cycle.c"],
 C031:["artifacts/reports/OSD-KERNEL-INIT-2026-10-31.md"],
 C032:["artifacts/reports/OSD-KERNEL-INIT-2026-10-31.md"],
 C046:["artifacts/reports/CEC-LIVE-2026-11-01.md"],
 C053:["common/svg_image.c","tests/svg_image.c"],
 C090:["common/svg_list_anim.c","tests/svg_list_anim.c"],
 C110:["modules/settings/settings.c","tests/settings_hotel.c"],
 C111:["artifacts/reports/CONTENT-HEADER-OWNERSHIP-2026-11-03.md"],
 C112:["modules/ui_navigation/ui_navigation.c","tests/ui_navigation.c"],
 C118:["tools/re/ui-object-scan.c","tests/ui_object_scan.c"],
 C119:["artifacts/reports/AGENT-WORK-REVIEW-2026-11-03.md"],
 C120:["tools/display/egl-glyph-probe.c"],
 C122:["artifacts/reports/UI-TITLE-LIVE-GRAPH-2026-11-03.md"],
 C125:["modules/ui_vendor/ui_vendor.c","tests/ui_settings_text.c"],
 C127:["tools/tvshot.py"], C128:["tools/tvshot.py"],
 C129:["artifacts/reports/UI-ASSETS-2026-10-30.md"],
 C132:["modules/ui_vendor/ui_vendor.c","tests/ui_sources.c"],
 C152:["artifacts/reports/UI-ROW-LABELS-2026-11-02.md"],
 C157:["common/font_pack.c","common/font_text.c","tests/ui_settings_text.c"],
 C164:["modules/ui_clock/ui_clock.c","tests/ui_clock.c"],
 C174:["modules/graphics/graphics.c","tests/scene_client.c"],
 C176:["modules/ui_vendor/ui_vendor.c","tests/ui_async.c"],
 C178:["common/canvas.c","tests/display_present.c"],
 C180:["artifacts/reports/STABILITY-2026-10-31.md"],
 C181:["AGENTS.md","artifacts/reports/DOCUMENTATION-AUDIT-2026-10-31.md"],
 C182:["artifacts/workbench/README.md","artifacts/scripts/extract_from_dump.sh"]
};
const manualReports={C008:"UI-ANIMATIONS",C017:"MODULE-SAFETY",C022:"MODULE-SAFETY",
 C023:"POWER-STANDBY-CONTRACT",C024:"POWER-STANDBY-CONTRACT",C046:"CEC-LIVE",
 C119:"AGENT-WORK-REVIEW",C127:"UI-PARITY",C128:"UI-PARITY",C129:"UI-ASSETS",
 C132:"UI-PARITY",C134:"UI-PARITY",C157:"UI-FONT-LIVE",C159:"UI-ENGINE-REVIEW",
 C164:"UI-ANIMATIONS",C182:"DOCUMENTATION-AUDIT"};
for(const e of entries) {
    const names=new Set();
    for(const file of reports) {
        const full=file.slice(0,-3),short=full.replace(/-20\d\d-\d\d-\d\d$/,"");
        if(e.text.includes(full) || e.text.includes(short))names.add(file);
    }
    for(const [rx,prefix] of extra)if(rx.test(e.title))
        for(const file of reports)if(file.startsWith(prefix+"-"))names.add(file);
    if(manualReports[e.id])for(const file of reports)
        if(file.startsWith(manualReports[e.id]+"-"))names.add(file);
    const evidence=[...names].sort();
    const haystack=[e.text,...evidence.map(f=>fs.readFileSync(path.join(reportDir,f),"utf8"))].join("\n");
    const source=new Set(),tests=new Set();
    for(const raw of manual[e.id]||[]) {
        const found=[raw,`openaurora/${raw}`].find(p=>fs.existsSync(path.join(root,p)));
        if(!found)throw Error(`manual anchor missing: ${e.id} ${raw}`);
        (found.includes("/tests/")?tests:source).add(found);
    }
    for(const m of haystack.matchAll(/(?:openaurora\/)?(?:common|core|modules|tests|tools|include\/openaurora)\/[\w./-]+\.(?:c|h|py|sh|cjs|java)\b/g)) {
        const raw=m[0];
        const options=[raw,`openaurora/${raw}`];
        const found=options.find(p=>fs.existsSync(path.join(root,p)));
        if(found)(found.includes("/tests/")?tests:source).add(found);
    }
    e.reports=evidence;e.sources=[...source].sort();e.tests=[...tests].sort();
}
fs.writeFileSync(path.join(__dirname,"before.md"),original);
fs.writeFileSync(path.join(__dirname,"claims.json"),JSON.stringify(entries,null,2)+"\n");
const md=["# Pre-audit checked-claim locators","",
    "These182 entries are from432c71f. Locators are not proof or automatic approval.",
    "The audit report records decisions, execution coverage and corrected scope.","",
    "| ID / original line | Original claim | Audit disposition | Report locators | Source / test/research locators |","|---|---|---|---|---|"];
for(const e of entries)md.push(`| ${e.id} / ${e.line} | ${e.title.replaceAll("|","\\|")} | ${e.disposition} | ${e.reports.map(f=>f.replace(/\.md$/," ")).join("; ")||"manual audit"} | ${[...e.sources,...e.tests].join("; ")||"manual audit"} |`);
fs.writeFileSync(path.join(__dirname,"checked-claims.md"),md.join("\n")+"\n");
console.log(JSON.stringify({checkedBefore:entries.length,checkedAfter:current.filter(e=>e.checked).length,
    reopened:entries.filter(e=>!e.current.checked).map(e=>e.id),wordingChanges:entries.filter(e=>e.current.checked&&e.text!==e.current.text).length,
    withoutReport:entries.filter(e=>!e.reports.length).map(e=>({id:e.id,title:e.title})),
    withoutSource:entries.filter(e=>!e.sources.length&&!e.tests.length).map(e=>({id:e.id,title:e.title}))},null,2));
