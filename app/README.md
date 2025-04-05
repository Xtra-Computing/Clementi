## Application Folder Structure

- ` clementi` : Implementation of the Clementi framework for FPGA-based graph processing.
- ` gather_scatter` : Pure gather-scatter module implementation in Clementi, designed to work on multiple FPGAs.
- ` global_apply` : Pure data updating implementation among FPGAs in Clementi (The apply stage is bypassed in this global_apply stage).
- ` single_gas` : Single FPGA implementation for a GAS-based edge-centric graph processing system.
- ` play_ground` : Unit tests in gather-scatter module.
- ` test_coalesce` : Unit tests in coalesce function.
- ` test_src_cache` : Unit tests in source cache functionality.