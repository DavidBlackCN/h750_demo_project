import { access, readFile, writeFile } from 'node:fs/promises';
import { constants } from 'node:fs';
import { basename, dirname, extname, join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

function usage() {
  console.error('Usage: node render_a4_svg.mjs <input.mmd> [output.svg]');
  process.exit(1);
}

const [inputArg, outputArg] = process.argv.slice(2);
if (!inputArg) usage();

const inputPath = resolve(inputArg);
const outputPath = outputArg
  ? resolve(outputArg)
  : join(dirname(inputPath), `${basename(inputPath, extname(inputPath))}.svg`);

try {
  await access(inputPath, constants.R_OK);
} catch {
  console.error(`Input file not found: ${inputPath}`);
  process.exit(1);
}

const codexHome = process.env.CODEX_HOME || join(process.env.USERPROFILE || 'C:/Users/Default', '.codex');
const packagePath = join(codexHome, 'skills', 'pretty-mermaid-skills', 'node_modules', 'beautiful-mermaid', 'dist', 'index.js');

try {
  await access(packagePath, constants.R_OK);
} catch {
  console.error('pretty-mermaid dependency is unavailable. Run the pretty-mermaid renderer once to install it.');
  process.exit(1);
}

const { renderMermaid, THEMES } = await import(pathToFileURL(packagePath).href);
const source = await readFile(inputPath, 'utf8');

// Preserve official Mermaid-compatible quoted labels in the source. The renderer
// receives an in-memory form without quotes and with wider Chinese measurements.
const renderInput = source
  .replace(/(\w+)\(\["([^"\n]*)"\]\)/g, '$1($2)')
  .replace(/(\w+)\["([^"\n]*)"\]/g, '$1[$2]')
  .replace(/(\w+)\{"([^"\n]*)"\}/g, '$1{$2}')
  .replace(/(subgraph\s+\w+)\["([^"\n]*)"\]/g, '$1[$2]')
  .replace(/\|"([^"\n]*)"\|/g, '|$1|')
  .replace(/[\u4e00-\u9fff]/g, '$&W');

let svg = await renderMermaid(renderInput, {
  ...THEMES['zinc-light'],
  font: 'Microsoft YaHei',
  transparent: false,
});

svg = svg.replace(/([\u4e00-\u9fff])W/g, '$1');
await writeFile(outputPath, svg, 'utf8');

const viewBox = svg.match(/viewBox="0 0 ([\d.]+) ([\d.]+)"/);
if (!viewBox) {
  console.log(`SVG written: ${outputPath}`);
  process.exit(0);
}

const width = Number(viewBox[1]);
const height = Number(viewBox[2]);
const ratio = width / height;
console.log(`SVG written: ${outputPath}`);
console.log(`Canvas: ${width.toFixed(0)} x ${height.toFixed(0)} (${ratio.toFixed(2)}:1)`);
if (ratio < 1.5 || ratio > 2.3) {
  console.warn('Warning: layout is outside the A4 landscape target ratio (1.5:1 to 2.3:1).');
}
