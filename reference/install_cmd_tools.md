# Use some lasR features from a terminal

Install the required files to be able to run some simple lasR commands
from a terminal. Working in a terminal is easier for simple tasks but it
is not possible to build complex pipelines this way. Examples of some
possible commands:

    lasr help
    lasr index -i /path/to/folder
    lasr vpc -i /path/to/folder
    lasr info -i /path/to/file.las
    lasr chm -i /path/to/folder -o /path/to/chm.tif -res 1 -ncores 8
    lasr dtm -i /path/to/folder -o /path/to/dtm.tif -res 0.5 -ncores 8

## Usage

``` r
install_cmd_tools()
```
