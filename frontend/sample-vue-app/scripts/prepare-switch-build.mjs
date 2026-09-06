import babel from '@babel/core';
import presetEnv from '@babel/preset-env';
import {parse} from 'acorn';
import {readdir, readFile, writeFile} from 'node:fs/promises';

const outputDirectory = new URL('../dist/', import.meta.url);
const assetsDirectory = new URL('assets/', outputDirectory);
const indexPath = new URL('index.html', outputDirectory);
const original = await readFile(indexPath, 'utf8');

if (!original.includes('type="module"')) {
  throw new Error('Vue output no longer contains a module entry script');
}

const moduleEntry = original.match(/\s*<script type="module"[^>]*src="([^"]+)"[^>]*><\/script>/);
if (!moduleEntry) {
  throw new Error('Could not locate the Vue module entry script');
}

const classicEntry = `<script src="${moduleEntry[1]}"></script>`;
const compatible = original
  .replace(moduleEntry[0], '')
  .replace('</body>', `  ${classicEntry}\n  </body>`);
await writeFile(indexPath, compatible);

for (const entry of await readdir(assetsDirectory)) {
  if (!entry.endsWith('.js')) continue;

  const scriptPath = new URL(entry, assetsDirectory);
  const source = await readFile(scriptPath, 'utf8');
  const transformed = await babel.transformAsync(source, {
    babelrc: false,
    comments: false,
    compact: true,
    configFile: false,
    presets: [[presetEnv, {bugfixes: true, modules: false, targets: {ie: '11'}}]],
    sourceType: 'script'
  });
  if (!transformed?.code) {
    throw new Error(`Babel produced no output for ${entry}`);
  }
  parse(transformed.code, {ecmaVersion: 5});
  await writeFile(scriptPath, transformed.code);
}
