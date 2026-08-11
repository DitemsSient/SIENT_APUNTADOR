---
name: new-rama
description: Creates a new feature branch from Master and switches to it. Usage: /new-rama <branch-name> (hint)
user_invocable: true
---

# New Feature Branch

Create a new feature branch from Master for isolated development.

## Syntax

```
/new-rama <branch-name> (hint)
```

- `<branch-name>` — nombre de la rama (sin `feature/`). Requerido.
- `(hint)` — nombre de la librería entre paréntesis. Opcional pero recomendado.
  Debe coincidir exactamente con un título `## hint` en `memorias_librerias.md`.

Ejemplos válidos:
- `/new-rama new_tones (buzzer)`
- `/new-rama fix_tracking (GPS)`
- `/new-rama refactor_menu (Menu)`

## Steps

1. Check `git status` for staged or unstaged changes. If the working tree is not clean,
   warn the user that they must commit or amend first, and stop.
2. Verify you are on the `Master` branch. If not, switch to it first.
3. Pull latest changes if a remote is configured (skip if no remote).
4. Create a new branch named `feature/<branch-name>` and switch to it.
5. **Memory context lookup** (only if a hint was provided):
   - Read `Core/Doc/memorias_librerias.md`.
   - Find the section whose heading `## <heading>` matches the hint
     (case-insensitive exact match on the heading word).
   - Extract that section's content (until the next `---` separator or end of file).
   - Display it to the user under a block labeled **"Contexto cargado desde memoria"**.
   - If no matching section is found, inform the user: "No se encontró la sección
     `## <hint>` en memorias_librerias.md".
6. Confirm: branch created and ready to work (plus context block if applicable).

## Rules

- If no argument is provided, ask the user for a branch name.
- If the branch already exists, warn the user and ask how to proceed.
- Do NOT make any commits or file changes — only create and switch branch.
- The hint lookup is an exact heading match, case-insensitive.
  Do not do fuzzy keyword search — the user controls the hint.
- Do not load `memorias_proyecto.md` from this skill; that file is for general
  project context only.