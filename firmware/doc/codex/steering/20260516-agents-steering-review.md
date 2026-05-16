# AGENTS steering review rule

## Purpose

Update `AGENTS.md` so future implementation work pauses after creating a
steering file and waits for user review before code changes begin.

## Scope

- `AGENTS.md`

## Main changes

- Add a review/approval step after steering file creation.
- Clarify that implementation starts only after the user confirms the steering
  file contents.

## Design notes

- Keep the existing workflow intact and only insert the new review gate.
- Do not change unrelated maintenance rules.

## Verification

- Review the markdown diff.
