# Contributing: git

These rules keep both Fiji repos (the bot, Fiji-Bot, and the OBS plugin, fiji-suite) easy to follow and safe to
release from. The same rules apply in both; only the test commands differ.

## Branches

- `main` is always releasable: it builds, its tests pass, and every release tag is on it. Nobody works on `main`
  directly.
- Work happens on a branch named for the work: `feature/<topic>`, `fix/<topic>`, `docs/<topic>`, `ci/<topic>`.
  Long-running branches like `architecture` are fine too.
- A branch reaches `main` through a merge (a Gitea pull request, or `git merge --no-ff` locally) once its tests pass.
  Darki does the merges. `main` is never rebased.
- Delete a branch once it has been merged.

## Commits

- One logical change per commit, and every commit builds and passes the tests. A fix for an earlier step goes in its
  own commit; it is not hidden in an unrelated one.
- Message: a short subject (72 characters at most) saying what changed, a blank line, then a body saying why and what
  to watch for. Plain sentences, not a list of files.
- Stage files by name, and look at `git status` and `git diff --staged` before committing.
- Never commit:
  - secrets: tokens, `bot.env`, `.git-credentials`;
  - build output or generated files (the plugin's `src/version.h`);
  - personal files: `.claude/`, loose images in the repo root.

## Before pushing

- Run the tests (below).
- Run `git fetch` first. A push must be a fast-forward. If the remote moved, rebase your own *unpushed* commits onto
  it (or merge it), then run the tests again.
- Never force-push `main` or a tag. Force-push your own branch only with `--force-with-lease`, and only when its
  history really had to change.

## Tests

```bash
python3 tools/check-architecture.py       # include rules, legacy list, bridge contract
cmake --build build_x86_64 --target fiji-arch-test fiji-core-test fiji-view-test decks-engine-test
ctest --test-dir build_x86_64 --output-on-failure
```

The plugin itself must build without warnings too: Windows treats warnings as errors.

## Tags and releases

- Releases are tagged on `main` only, with annotated semver tags: `git tag -a vX.Y.Z -m "X.Y.Z"`.
- The version bump and the CHANGELOG go on the branch before it is merged. The CHANGELOG's "Unreleased" section
  becomes the version's section. After the merge, the tag goes on `main`'s merge commit and is pushed.
- Plugin: the version lives in `buildspec.json`, `data/manifest.json`, the Flatpak `metainfo.xml` and
  `CHANGELOG.md`. Pushing the tag makes CI build Windows, Linux and Flatpak and publish the release.
- A pushed tag is never moved or deleted. A broken release gets a new patch version.

## The bridge contract

`BRIDGE_CONTRACT.md` is shared by both repos. A change to it lands in both, in matching commits (the tests compare
the two copies).

## What Claude does on its own, and what it asks first

- **On its own:**
  - commits finished, tested work on a work branch, and pushes that branch;
  - `git fetch`;
  - releases: bumps the version and the CHANGELOG on the branch. Once Darki has merged it into `main`, Claude tags
    `main`'s merge commit `vX.Y.Z`, pushes the tag, and says which tag went out.
- **Only Darki:** merging into `main`.
- **Asks first:**
  - pushing commits to `main`;
  - any force-push;
  - deleting branches;
  - rewriting pushed history.
- **Never:** moving or deleting a pushed tag.
