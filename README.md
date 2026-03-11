# Validose Mobile App

Cross-platform mobile companion app for the Validose system, connecting to dock/ring hardware over BLE to run dosing workflows, manage schedules, and sync dose events.

## Prerequisites

- Node.js `>=18`
- npm
- Xcode (iOS builds) and/or Android Studio (Android builds)
- EAS CLI account access (`EXPO_TOKEN`) for cloud builds/submissions

## Quick Start

1. Install dependencies:

```bash
npm install
```

2. Run the app:

```bash
npm run android
# or
npm run ios
```

3. Start Metro manually (optional):

```bash
npm start
```

## Common Scripts

- `npm start`: start Expo with default `.env`
- `npm run start:dev`: start Expo with `.env.development`
- `npm run start:staging`: start Expo with `.env.staging`
- `npm run start:prod`: start Expo with `.env.production`
- `npm run android`: build/run Android app locally
- `npm run ios`: build/run iOS app locally
- `npm test`: run Jest tests
- `npm run lint`: run ESLint
- `npm run lint-fix`: run ESLint with auto-fix
- `npm run prettier`: format codebase
- `npm run version:build:show`: print current iOS/Android build numbers from `app.json`
- `npm run version:build:bump -- 1`: increment both build numbers (`ios.buildNumber`, `android.versionCode`)
- `npm run version:build:set -- 120`: set both build numbers to a specific value
- `npm run build:android`: local Android release build (APK)
- `npm run build:ios`: production iOS build

## Environment Files

1. Copy `.env.example` to `.env.development`, `.env.staging`, and/or `.env.production` (as needed).
2. Fill values for each environment.
3. Use scripts that set `ENV_FILE` (for example `npm run start:staging`) so Babel compiles with the right variables.

## Build And Release (EAS)

- Preview/internal builds:
```bash
npm run build:android:preview
npm run build:ios:preview
```
- Store-ready production builds:
```bash
npm run build:android:production   # AAB
npm run build:ios:production
```
- Local Android release artifact:
```bash
npm run build:android:production:local   # outputs build/android-release.apk
```
- Submit existing store binaries:
```bash
npm run submit:android:production
npm run submit:ios:production
```
- Build + auto-submit in one command:
```bash
npm run release:android:production
npm run release:ios:production
```

## Project Structure (Current)

```text
validose-mobile-app/
├── src/
│   ├── app/                    # Expo Router screens
│   ├── components/             # Shared UI components
│   ├── providers/              # Auth/log providers
│   ├── services/               # API/hardware integrations
│   ├── store/                  # Zustand stores
│   ├── types/                  # Shared TS types
│   └── utils/
│       └── ble/                # BLE + Message Protocol stack
├── modules/                    # Native BLE bridge module
├── android/
├── ios/
├── app.json
└── package.json
```

## BLE Message Protocol Docs

BLE implementation details live in:

- [`src/utils/ble/README.md`](./src/utils/ble/README.md)

That README covers:

- message protocol framing/sync/ack-retry model
- PPI enum and payload decoding alignment with firmware
- app runtime integration (`connectionHandling`)
- debug console/screen/log flow

## Android Release Build Setup

For Android release builds:

1. Rename properties template:

```bash
android/gradle.properties.example -> android/gradle.properties
```

2. Set credentials in `android/gradle.properties`:

```bash
VALIDOSE_RELEASE_STORE_FILE=validose-release-key.keystore
VALIDOSE_RELEASE_KEY_ALIAS=your-alias
VALIDOSE_RELEASE_STORE_PASSWORD=YOUR_PASSWORD
VALIDOSE_RELEASE_KEY_PASSWORD=YOUR_PASSWORD
```

3. Copy keystore into app module:

```bash
validose-release-key.keystore -> android/app/
```

## CI/CD Ownership

- Release runbook: [`docs/release-runbook.md`](./docs/release-runbook.md)
- Manual release workflow: `Actions -> Build And Distribute Mobile`

- Local machine:
Use local commands for testing credentials, ad-hoc/internal builds, and emergency/manual releases.
- GitHub Actions:
Use for repeatable production release jobs, store submissions, and protected-branch automation.

Recommended split:
1. Keep `build` and `submit` commands available locally.
2. Use GitHub Actions as the default path for production releases.
3. Keep local release scripts as fallback when CI is unavailable.
