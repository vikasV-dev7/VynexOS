# Initial filesystem (`--vfs-profile`)

The system's filesystem lives in memory and is filled at startup by a *profile*, applied right after it is created
(`src/bootstrap/vfs_profiles.*`, through the plain `IVirtualFileSystem` interface and the generic helper
`core::seed_filesystem` in `include/vynexos/core/vfs_seed.hpp`). The filesystem classes themselves know nothing about
profiles.

| Profile | Contents |
|---|---|
| `development` (default) | `docs/` (with `getting-started.md`), `music/`, `pictures/`, `notes.md`, `readme.txt`, `todo.txt` |
| `empty` | nothing (File Explorer shows `Items: 0`) |

A plain launch (F5 in Visual Studio, `vynex_init.exe`, a double click) uses `development`; nothing depends on the working
directory. To choose another profile pass `--vfs-profile=empty` or `--vfs-profile=development`. In Visual Studio add
`"args": ["--vfs-profile=empty"]` to the `vynex_init` entry of `.vs/launch.vs.json`. An unknown profile or argument stops
startup with a message and exit status 2.

Seeding never overwrites: an entry that already exists is left as it is (a file keeps its contents), a conflict between a
file and a directory is reported, and running a profile twice creates nothing the second time.

Tests build their own filesystems and must not depend on these profiles. Only `StartupProfileTests` pins the profile
contents (exact file texts included), so changing a profile is a deliberate change to that test.
