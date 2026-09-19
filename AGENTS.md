# Working Rules

- Documentation lives in `README.md`. Read only the sections and linked docs needed for the task.
- Use `rg` to locate affected code and callers; avoid reading entire files or generated/vendor trees unnecessarily.
- Check `git status` first. Preserve existing changes and keep patches focused. Never edit generated files or dependencies.
- Follow existing style. Update affected callers, tests, build registration, and documentation together.
- Preserve input validation, motor safety, protocol behavior, and resource cleanup unless the task explicitly changes them.
- Keep `.sh` and `.ps1` wrappers synchronized through shared Python implementations.
- Never open serial ports, flash, or run hardware tests without authorization and confirmed board/port. Keep one serial owner; stop motors on exit.
- Add focused tests for changed logic. Run relevant checks documented in `README.md`; use host-only tests unless hardware is authorized.
- Format touched source files and review `git diff --check`. Documentation-only edits do not need builds.
- Report what changed, check results, and skipped checks with reasons. Do not claim host tests validate hardware.
