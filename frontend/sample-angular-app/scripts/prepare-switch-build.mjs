import babel from '@babel/core';
import presetEnv from '@babel/preset-env';
import {parse} from 'acorn';
import {readdir, readFile, writeFile} from 'node:fs/promises';

const outputDirectory = new URL('../dist/sample-angular-app/', import.meta.url);
const indexPath = new URL('index.html', outputDirectory);
const original = await readFile(indexPath, 'utf8');

if (!original.includes('type="module"')) {
  throw new Error('Angular output no longer contains module entry scripts');
}

const compatible = original.replace(/ type="module"/g, '');
await writeFile(indexPath, compatible);

for (const entry of await readdir(outputDirectory)) {
  if (!entry.endsWith('.js')) continue;

  const scriptPath = new URL(entry, outputDirectory);
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
