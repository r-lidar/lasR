# lasR 0.2.0

- New: `rasterize()` gains the ability to perform a multi-resolution or buffered rasterization. See documentation.
- New: the internal engine gains the ability to skip the processing of some file of the collection and use these files only to load a buffer. This feature works with a `LAScatalog` from `lidR` respecting the `processed` attribute used in `lidR`

# lasR 0.1.2

- New: progress bar when reading the header of the files (`LAScatalog`) can be enabled with `progress = TRUE`
- Fix: progress bar starts to appear earlier i.e. from 0%. For some pipeline it affects the feeling of progress.

# lasR 0.1.1 (2024-02-08)

- Doc: Corrected the documentation for the argument `ncores` in `processor()`, which incorrectly mentioned that it was not supported.
- New: Added new functions `ncores()` and `half_cores()`.
- Fix: Corrected the reader progress bar display when reading a las file with a filter and a buffer.
- Fix: Fixed the overall progress bar, which was delayed by one file and was showing incorrect progress.