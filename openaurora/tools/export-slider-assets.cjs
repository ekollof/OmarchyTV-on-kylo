/* Export shipped slider and selection PNGs to local OAI1.
 * Usage: node tools/export-slider-assets.cjs PNG_DIRECTORY UI_DIRECTORY
 * Requires ImageMagick; no vendor image bytes are embedded in project source. */
"use strict";
const fs = require("node:fs");
const path = require("node:path");
const {execFileSync} = require("node:child_process");
if (process.argv.length !== 4) throw Error("usage: export-slider-assets.cjs PNG_DIRECTORY UI_DIRECTORY");
const [input, output] = process.argv.slice(2);
fs.mkdirSync(output, {recursive: true});
for (const [name, w, h] of [["slidebarfa", 24, 24], ["slidebarufa", 24, 24],
                            ["select_left", 18, 24], ["select_right", 18, 24]]) {
    const source = path.resolve(input, `${name}.png`);
    const dimensions = execFileSync("magick", ["identify", "-format", "%w %h", source], {encoding: "utf8"});
    if (dimensions !== `${w} ${h}`) throw Error(`${source}: expected ${w}x${h}, got ${dimensions}`);
    const rgba = execFileSync("magick", [source, "-depth", "8", "rgba:-"]);
    if (rgba.length !== w * h * 4) throw Error(`${source}: invalid RGBA byte count`);
    const header = Buffer.alloc(12);
    header.write("OAI1"); header.writeUInt32LE(w, 4); header.writeUInt32LE(h, 8);
    const destination = path.join(output, `${name}.rgba`);
    fs.writeFileSync(destination, Buffer.concat([header, rgba]));
    console.log(`${destination}: ${w}x${h} RGBA`);
}
