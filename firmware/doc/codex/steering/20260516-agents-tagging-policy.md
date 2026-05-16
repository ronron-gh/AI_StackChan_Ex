# AGENTS tagging policy

## Purpose

Add a standard tagging policy to `AGENTS.md` so release tags can be created
consistently without repeating the workflow details each time.

## Scope

- `AGENTS.md`

## Main changes

- Add a tag policy section.
- Clarify that tags are created only when the user explicitly requests them.
- Use annotated tags for release tags.
- Use `vX.Y.Z` as the standard release tag name format.
- Use the release comment provided by the user, without forcing it to be one
  line.
- Push tags only when the user explicitly requests push.

## Design notes

- Keep the user's timing control: no automatic tagging after merges or version
  bumps.
- Do not prescribe fixed release note text because the user will provide it per
  release.

## Verification

- Review the markdown diff.
