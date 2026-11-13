// Writes tests/fixtures/plottracer/curve-export.json with PlotTracer's own
// buildSeriesJSON, so the importer is tested on the real format. Run from a
// PlotTracer checkout (it needs PlotTracer's engine and vitest):
//   cd plottracer && PLOTTRACER=$PWD \
//     TF_OUT=<TenForge>/tests/fixtures/plottracer/curve-export.json \
//     node_modules/.bin/vitest run --root <TenForge>/tools/fixtures --dir <TenForge>/tools/fixtures
import { test } from 'vitest';
import { writeFileSync } from 'node:fs';

test('write', async () => {
  const { buildSeriesJSON } = await import(process.env.PLOTTRACER + '/engine/csvExport.ts');
  // A curve as a digitised figure would give (strain %, stress MPa), made up
  // for the fixture, with one unmeasured point (null) and a second series
  // that is an error bar of the first.
  const pts: [number, number | null][] = [[0, 0], [0.2, 140], [0.4, 255], [0.6, 278], [1, 290], [2, 302], [4, 315], [6, 322], [8, 325], [9, null], [10, 326]];
  const row = (v: (number | null)[]) => ({ px: 0, py: 0, values: v });
  const json = buildSeriesJSON(
    [
      { name: 'aluminium-like curve, made up for the fixture', rows: pts.map(([x, y]) => row([x, y])) },
      { name: 'upper bound', rows: [row([4, 330])], relation: { role: 'upper', of: 'aluminium-like curve, made up for the fixture' } },
    ],
    ['Strain (%)', 'Stress (MPa)'],
  );
  writeFileSync(process.env.TF_OUT!, json + '\n');
});
