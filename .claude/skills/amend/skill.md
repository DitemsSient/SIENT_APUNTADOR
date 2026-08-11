---
name: amend
description: Amend staged changes into the previous commit without creating a new one. Usage: /amend
user_invocable: true
---

# Amend Previous Commit

Fold current staged changes into the last commit, keeping the same message.

## Steps

1. Run `git status` to check for staged changes.
2. If there are no staged changes, warn the user and stop.
3. Show the user what is staged (`git diff --cached --stat`) and the last commit message (`git log -1 --oneline`).
4. Ask the user to confirm they want to amend these changes into the previous commit, or if they want to modify the commit message.
5. Run `git commit --amend --no-edit` (or with the new message if the user provided one).
6. Confirm to the user: changes amended into the previous commit.

## Rules

- If there are no staged changes, warn and stop.
- If there are unstaged changes, only amend what is staged — do NOT auto-stage anything.
- Do NOT push to remote unless the user explicitly asks.
