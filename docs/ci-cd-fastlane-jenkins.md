# CI/CD Setup (Fastlane + Jenkins)

This repo is configured to deploy Android and iOS using Fastlane from Jenkins, with environment-aware behavior.

## Environment Mapping

By default (`DEPLOY_ENV=auto`):

- `develop` -> `stage`
- `main` -> `prod`
- any other branch -> `temp` (allowed only when `ALLOW_FEATURE_BRANCH_DEPLOY=true`)

Manual override is supported with Jenkins parameter `DEPLOY_ENV` (`stage`, `prod`, `temp`).

## Backend Base URL Strategy

`BASE_URL` is injected at build time via `ENV_FILE` and consumed in the app bundle.

Resolution order:

1. `BACKEND_BASE_URL` Jenkins parameter (if set)
2. Jenkins defaults in `Jenkinsfile`:
   - `DEFAULT_STAGE_BASE_URL`
   - `DEFAULT_PROD_BASE_URL`
   - `DEFAULT_TEMP_BASE_URL`

## Build Labels

Each build gets a release label:

- format: `<env>-<branch>-<build-number>`
- Jenkins build display name uses this label
- Android produces labeled AAB artifact (`build/android/app-<label>.aab`)
- iOS produces labeled IPA artifact (`build/ios/validose-<label>.ipa`)
- TestFlight changelog includes env + label

## Release Notes / Change Description

Change description is stored in all available points:

- Jenkins build description (short form)
- Jenkins artifact file: `build/metadata/release-notes.md` (full form)
- TestFlight changelog (full/truncated to `IOS_CHANGELOG_MAX_CHARS`)
- Google Play release notes (`en-US` by default, truncated to `ANDROID_RELEASE_NOTES_MAX_CHARS`)

Source of release notes:

1. Optional Jenkins parameter `CHANGE_DESCRIPTION` (manual notes)
2. Auto-generated commit summary from git log

These are combined automatically during pipeline execution.

## Files Added/Updated for CI/CD

- `Gemfile`
- `fastlane/Fastfile`
- `fastlane/Appfile`
- `fastlane/.env.example`
- `Jenkinsfile`

## Jenkins Agent Requirements

Use a macOS Jenkins node (label `macos-mobile`) with:

- Xcode + command line tools
- CocoaPods
- Ruby (with `gem` install access)
- Node.js + npm
- Android SDK and Java 17

## Jenkins Parameters

Defined in `Jenkinsfile`:

- `DEPLOY_ANDROID`, `DEPLOY_IOS`
- `DEPLOY_ENV` (`auto|stage|prod|temp`)
- `BACKEND_BASE_URL`
- `CHANGE_DESCRIPTION`
- `ALLOW_FEATURE_BRANCH_DEPLOY`
- `UPLOAD_TO_STORES` (for stage/prod)
- `UPLOAD_TEMP_TO_STORES` (for temp)
- `ANDROID_TRACK` (non-prod default `internal`)
- `MATCH_GIT_URL` (optional)

## Jenkins Credentials to Create

Always required when Android build runs:

- `validose-android-keystore` (Secret file)
- `validose-android-key-alias` (Secret text)
- `validose-android-store-password` (Secret text)
- `validose-android-key-password` (Secret text)

Required only when Android store upload is enabled:

- `validose-google-play-json` (Secret file)

Required only when iOS store upload is enabled:

- `validose-appstore-key-id` (Secret text)
- `validose-appstore-issuer-id` (Secret text)
- `validose-appstore-api-key-p8` (Secret file)

Required only when `MATCH_GIT_URL` is set:

- `validose-match-git-basic-auth` (Secret text)
- `validose-match-password` (Secret text)

## iOS Signing (Match)

If `MATCH_GIT_URL` is set, Fastlane runs `match` for app-store profiles.
If omitted, Jenkins macOS node must already have valid signing certs/profiles installed.

## Local Commands

```bash
bundle install
bundle exec fastlane lanes
bundle exec fastlane ci_checks
```

Dry runs without store uploads:

```bash
ANDROID_UPLOAD_TO_STORE=false bundle exec fastlane android develop
IOS_UPLOAD_TO_STORE=false IOS_BUILD_NUMBER=1 bundle exec fastlane ios develop
```

## Good Practice Notes

- Keep `DEPLOY_ENV=auto` for normal workflow and branch governance.
- Use `ALLOW_FEATURE_BRANCH_DEPLOY=true` only for temporary branch builds.
- Keep `UPLOAD_TEMP_TO_STORES=false` unless explicitly needed.
- Use `BACKEND_BASE_URL` only for exceptional/manual overrides.
- Never commit signing keys or store credentials.
