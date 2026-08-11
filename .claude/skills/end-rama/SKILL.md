---
name: end-rama
description: Commits changes, merges current feature branch into Master, and deletes the feature branch. Usage: /end-rama
user_invocable: true
---

# Finish Feature Branch

Wrap up the current feature branch by committing, merging to Master, and cleaning up.

## Steps

1. Verify you are on a `feature/*` branch. If not, warn the user and stop.
2. Run `git status` and `git diff` to see all changes.
3. Run `git log --oneline Master..HEAD` to see commits already made on this branch.
4. Stage all relevant files (avoid .env, credentials, or other sensitive files).
5. Ask the user to confirm the commit message or provide one. Draft a descriptive commit message in Spanish following the project's commit style (detailed description of what was added/changed and why).
6. Switch to `Master`.
7. Merge the feature branch into Master using `git merge --no-ff feature/<name>` to preserve branch history.
8. Delete the feature branch: `git branch -d feature/<name>`.
9. Confirm to the user: merged and cleaned up.

## Rules

- If there are no changes to commit and no commits ahead of Master, warn the user and stop.
- If there are merge conflicts, inform the user and help resolve them.
- Do NOT push to remote unless the user explicitly asks.
- If there are already commits on the branch but no uncommitted changes, skip to step 7 (merge).