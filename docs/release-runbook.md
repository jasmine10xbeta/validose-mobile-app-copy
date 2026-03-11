# Release Runbook

This runbook defines how to build from any branch, store build artifacts on GitHub, optionally submit to one or both stores, and preserve change notes.

## Workflow

- GitHub Actions workflow: `.github/workflows/release.yml`
- Workflow name in UI: `Build And Distribute Mobile`
- Trigger type: manual (`workflow_dispatch`)

## What This Flow Supports

- Build Android, iOS, or both.
- Build from any branch/tag/SHA via `target_ref`.
- Upload build metadata and downloaded binaries to GitHub Actions artifacts.
- Create a Git tag for the build.
- Create a GitHub pre-release that includes:
  - release notes,
  - changelog input,
  - build metadata JSON,
  - downloaded binary assets (when available).
- Optionally submit to Android store, iOS store, or both.

## Required GitHub Secrets

- `EXPO_TOKEN`
  - Required for EAS build/submit commands.
- `MDK_GITHUB_ACCESS_TOKEN` (optional if default token can access everything)
  - Needed when private submodules require extra access.

## Required EAS / Store Setup

- EAS project configured and linked (`eas.json` + Expo project access).
- EAS submit credentials configured:
  - Android Play service account + signing.
  - iOS App Store Connect API key + signing credentials.

## Recommended Environment Gate

Use GitHub environment `production-release`:

1. Open repository `Settings -> Environments`.
2. Create `production-release`.
3. Add required reviewers.
4. Keep release workflow bound to this environment (already configured).

## Workflow Inputs

- `target_ref`: branch/tag/SHA to build (any branch supported).
- `build_platform`: `android`, `ios`, `both`.
- `store_target`: `none`, `android`, `ios`, `both`.
- `build_profile`: EAS build profile (`development`, `preview`, `production`, `production-apk`).
- `submit_profile`: EAS submit profile (default `production`).
- `run_checks`: run lint/tests before building.
- `create_tag`: create/push build tag and GitHub pre-release.
- `tag_prefix`: prefix for generated tag.
- `build_label`: human-readable label used in tag/release metadata.
- `changes_summary`: changelog text captured in GitHub metadata and release notes.
- `artifact_retention_days`: retention for Actions artifacts.

## Important Constraints

- If `store_target != none`, the workflow enforces `build_profile=production`.
- Store release notes are not automatically written to App Store/Play listing text by EAS submit; `changes_summary` is stored in GitHub release notes + metadata and included in EAS build message for traceability.

## How To Run

1. Open `Actions -> Build And Distribute Mobile`.
2. Click `Run workflow`.
3. Select the branch containing this workflow file.
4. Set:
   - `target_ref` to the branch/commit you want to build.
   - `build_platform` and `store_target` as needed.
   - `changes_summary` with what changed in this build.
5. Run and approve `production-release` environment when prompted.
6. Verify outputs:
   - Actions artifact bundle (`build-artifacts/*` + `metadata/*`).
   - Git tag and GitHub pre-release (if `create_tag=true`).
   - Store submissions in EAS/console (if `store_target != none`).

## Local Fallback

1. `eas login`
2. Optional: `npm run version:build:bump -- 1`
3. Build only:
   - `npm run build:android:production`
   - `npm run build:ios:production`
4. Build + submit:
   - `npm run release:android:production`
   - `npm run release:ios:production`
