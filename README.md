# Validose Mobile App

Cross-platform mobile companion app for the Validose system, connecting to dock/ring hardware over BLE to run dosing workflows, manage schedules, and sync dose events.

## Prerequisites

- Node.js `>=18`
- npm
- Xcode (iOS builds) and/or Android Studio (Android builds)

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

- `npm run android`: build/run Android app
- `npm run ios`: build/run iOS app
- `npm start`: start Expo dev server
- `npm test`: run Jest tests
- `npm run lint`: run ESLint
- `npm run lint-fix`: run ESLint with auto-fix
- `npm run prettier`: format codebase

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

## CI/CD

Fastlane + Jenkins pipeline docs:

- [`docs/ci-cd-fastlane-jenkins.md`](./docs/ci-cd-fastlane-jenkins.md)

Branch mapping:

- `develop` -> `stage`
- `main` -> `prod`
- feature branches -> `temp` (guarded via Jenkins parameter)
