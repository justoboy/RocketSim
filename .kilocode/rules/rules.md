# Project Rules: custombot

## OPERATIONAL CONSTRAINTS & COMMAND SAFETY

You may run specific non-destructive shell commands (`git`, `dir`, `copy`, compilation/build commands) strictly within the project workspace root. You must adhere to the following safety boundaries:

1. ABSOLUTE PATH ISOLATION
- Never use directory traversal sequences (e.g., `..\\`, `../`) to escape the workspace.
- Explicitly forbidden paths: `C:\\Windows\\*`, `C:\\Program Files\\*`, AppData, and user profile roots.

2. FORBIDDEN FILE-WRITING SYNTAX
- Do not use shell redirection operators (`>`, `>>`) or piping (`|`) to create or modify files outside of the workspace from the command 
  line.
- Never invoke .NET file-writing methods (e.g., `[System.IO.File]`) or PowerShell content cmdlets (`Set-Content`, `Add-Content`, `Out-File`).

3. SYSTEM & DESTRUCTIVE COMMAND BLACKLIST
- Strictly forbidden commands: `del`, `rmdir`, `rd`, `format`, `shutdown`, `taskkill`, `reg`, `sc`, `schtasks`, `bcdedit`, `net user`.
- Clean operations: Never force-delete files via terminal. To clean build artifacts, rely exclusively on native build tool targets (e.g., `cmake --build . --target clean`).

4. ENVIRONMENT & EXFILTRATION CONTROLS
- Never run global installation commands (`npm install -g`, `pip install` without a venv, windows installers).
- Network isolation: Do not use `certutil`, `bitsadmin`, `curl`, or `wget` to pull external binaries or assets unless explicitly requested by the user.

5. STATE PRESERVATION & ELEVATED RISK
- If a command alters project configurations, build files (CMakeLists.txt), or source files significantly, you must stage/commit 
  existing changes via `git` first so you can rollback if something goes wrong.
- You must halt execution and explicitly prompt the user for permission before running any command that modifies system state or environment variables.


## Git backups (run via scratch.bat)

- Before any major/risky code change, commit a backup snapshot.
- After completing a major change, commit again with a descriptive message.


## Todo tracking

- Always use markdown checklists in `update_todo_list` (lines like `- [x] done` /
  `- [ ] pending`), never JSON arrays.
- Keep the checklist updated at each step; the todo list is the durable record of
  progress since chat history can be lost.


## General

- Windows + cmd.exe environment; use cmd-compatible commands.
- Prefer the `read_file` tool for reading files over shell commands.
