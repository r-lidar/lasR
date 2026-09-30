# COPC LOD visual preview — 2026-05-04

Open this file in Markdown preview from `benchmarks/copc/`.

## Experimental writer, current normal preset

![lasR experimental reopened shallow LOD composite](results/lod_visual_lasR_experimental_reopen_xy/lod_composite.png)

Current benchmark output using XY-balanced shallow LOD routing plus reopened intermediate XY octants after resident-budget flushes. Visual uniformity is GOOD at depths 0-5, and the previous horizontal underfilled bands at depths 3-4 are no longer visible in the preview.

Clean benchmark, 2026-05-05, one warm repeat per writer:

| writer | wall_s warm | peak RSS MB | output MB |
|---|---:|---:|---:|
| lasR-experimental | 48.89 | 588.5 | 275.7 |
| lasR-experimental-dense | 50.57 | 619.0 | 272.1 |
| untwine | 54.51 | 745.5 | 269.5 |

Depth totals for the normal experimental preset now closely match the previous high-RAM protected-depth trial at the middle depths:

| depth | points |
|---:|---:|
| 0 | 65,567 |
| 1 | 277,170 |
| 2 | 1,094,687 |
| 3 | 4,439,612 |
| 4 | 14,207,380 |
| 5 | 25,158,919 |

Raster uniformity at 5 m improved from the old normal preset CV of 0.387 at depth 3 and 0.328 at depth 4 to 0.166 and 0.209 respectively, without raising the resident writer budget.

## Experimental writer, previous normal preset

![lasR experimental previous LOD composite](results/lod_visual_lasR_experimental/lod_composite.png)

Previous XY-balanced shallow LOD routing before reopen-on-flush. It improved the original scan-order artifact, but still showed residual horizontal underfilled bands at depths 3-4.

## Experimental writer, 768 MB resident-budget trial

![lasR experimental resident 768 MB LOD composite](results/lod_visual_lasR_experimental_res768/lod_composite.png)

Best middle-ground trial so far: cleaner depths 3-4 with about 1.02 GB RSS on the benchmark tile.

## Experimental writer, protected depth 4 trial

![lasR experimental protected d4 XY LOD composite](results/lod_visual_lasR_experimental_protect_d4_xy/lod_composite.png)

Cleanest shallow-depth image tested, but expensive: about 1.65 GB RSS on the benchmark tile.

## Experimental writer, protected shallow LODs (superseded)

![lasR experimental protected LOD composite](results/lod_visual_lasR_experimental_protect_d2/lod_composite.png)

Early protected-depth attempt kept for comparison; it reduced the strongest stripes but did not fix the shallow-depth distribution as cleanly.

## Experimental writer, XY-balanced shallow LODs (scratch confirmation)

![lasR experimental XY-balanced LOD composite](results/lod_visual_lasR_experimental_xy_lod/lod_composite.png)

## Untwine comparison

![Untwine LOD composite](results/lod_visual_untwine/lod_composite.png)

## Experimental writer, dense preset

![lasR experimental dense LOD composite](results/lod_visual_lasR_experimental_dense/lod_composite.png)

## Report images

### Single tile

![Single-tile LOD composite](results/multitile/lod_visual_single/lod_composite.png)

### Merged 2 x 2 tile COPC

![Merged-tile LOD composite](results/multitile/lod_visual_merged/lod_composite.png)

### Edge-artifact check

![Edge artifact check](results/multitile/edge_check/edge_artifact_check.png)
