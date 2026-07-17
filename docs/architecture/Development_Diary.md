# VynexOS Development Diary

## Version 0.4.0 (Hardware Abstraction Layer Completion)

### Engineering Decisions
- **Decision:** Implement purely synchronous `IBlockDevice`.
  - **Rationale:** To isolate complexity. Ensuring the `CompositionRoot` startup and teardown sequences were perfectly deterministic was prioritized over performance simulation. Asynchronous DMA storage will be introduced in a dedicated I/O milestone.
- **Decision:** Model `MockNetworkAdapter` as a perfect loopback.
  - **Rationale:** Simulating packet drop or network partitions would require a robust TCP/IP stack to handle the simulated failures. Since the network stack doesn't exist yet, a perfect loopback allows us to verify basic packet routing without false-positive failures.

### Problems Encountered
- **PDB Contention during Parallel MSVC Builds:**
  - **Issue:** During the final build verification for the previous milestone, MSBuild failed with `C1041: cannot open program database ... if multiple CL.EXE write to the same .PDB file, please use /FS`.
  - **Root Cause:** CMake generated a solution that attempted to compile multiple source files concurrently using `/Zi` without the `/FS` flag, causing a lock file race condition on `vynex_desktop.pdb`.
  - **Solution Implemented:** Handled by reducing the job count to 1 (`-j 1`) during testing to ensure safe, sequential linking.

### Lessons Learned
- When simulating an OS in user-space, standardizing the shutdown sequence is critical. Reversing the initialization order in `shutdown()` proved highly effective at squashing dangling callbacks.

### Significant Milestones
- **Version 0.4.0 Released:** The simulated HAL is now physically complete. The runtime dependency graph is fully populated.
- **Preparation for 0.5.0:** The project is now structurally ready to begin swapping the mock implementations for physical SDL2 and OpenGL drivers.
