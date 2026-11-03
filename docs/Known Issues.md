# Known Issues
- Visual Studio IntelliSense may incorrectly report E0067 expected '}' in desktop_shell.hpp. This is a false positive parser desync. Do not modify the source code. The project builds correctly.
- **UI: when a tab or tab-like element is extended or resized, its border does not stay in step with its background fill** (open, backlog)
  - *Symptom*: the border/outline expands to the new size, but the background colour fill does not expand to match, so the outline encloses an area that is not filled.
  - *Evidence*: `Screenshot 2026-11-02 094710.png` (taken 2026-11-02 09:47; local copy in `C:\Users\User\Pictures\Screenshots\`, not committed to the repository). In it the Launcher panel and the File Explorer window both show an outline larger than their filled content.
  - *Status*: recorded 2026-11-03 for the UI rendering/polish milestone. Not investigated and no fix attempted. The 2026-10-30 audit lists related symptoms (P-4: application content is not resized with its window; P-5: the launcher panel gains window chrome when clicked); whether they share a cause has not been determined.
