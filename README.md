# Validose Mobile Application (Expo)

This is an [Expo](https://expo.dev) project created with [`create-expo-app`](https://www.npmjs.com/package/create-expo-app).

## Get started

1. Install dependencies

   ```bash
   npm install
   ```

2. Start the app

   ```bash
    npm run android/ios
   ```

Validose is a mobile application designed to help users manage and track their medications effectively. Built with Expo, it offers a cross-platform solution for both iOS and Android devices.

## Table of Contents

- [Validose Mobile Application (Expo)](#validose-mobile-application-expo)
  - [Get started](#get-started)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [Getting Started](#getting-started)
  - [File Structure](#file-structure)
  - [🔐 Android Release Build Setup](#-android-release-build-setup)
  - [🚀 CI/CD (Fastlane + Jenkins)](#-cicd-fastlane--jenkins)
  - [Dependencies](#dependencies)
  - [Contributing](#contributing)
  - [License](#license)

## Features

- **Medication Tracking:** Log and monitor your medication intake.
- **Reminders:** Set up reminders to take medications on time.
- **Dosage Information:** Store dosage details for each medication.
- **User Authentication:** Secure user accounts with login and registration.
- **Data Visualization:** View medication history through charts and graphs.

## Getting Started

1.  **Prerequisites:**

    - Node.js (>=18)
    - npm or yarn
    - Expo CLI (`npm install -g expo-cli`)
    - Expo Go app on your iOS or Android device (for development)

2.  **Installation:**

    Clone the repository:

    ```bash
    git clone <repository-url>
    cd validose-mobile-app
    ```

3.  **Install Dependencies:**

    ```bash
    npm install
    # or
    yarn install
    ```

4.  **Run the App:**

    ```bash
    npm start
    # or
    yarn start
    ```

    This will start the Expo development server. You can then scan the QR code with the Expo Go app on your phone or run it in an emulator.

## File Structure

Here's a breakdown of the project's file structure:

```bash
 validose-mobile-app/
 ├── App.js                  # Main entry point of the application
 ├── app.json                # Expo configuration file
 ├── assets/                 # Static assets (images, fonts, etc.)
 │   ├── fonts/              # Custom fonts
 │   ├── images/             # Images used in the app
 │   └── ...
 ├── components/             # Reusable UI components
 │   ├── Button.js           # Example: Custom button component
 │   ├── Card.js             # Example: Card component
 │   └── ...
 ├── navigation/             # Navigation configurations
 │   ├── AppNavigator.js     # Main app navigator
 │   └── ...
 ├── screens/                # Application screens
 │   ├── HomeScreen.js       # Home screen
 │   ├── LoginScreen.js      # Login screen
 │   ├── MedsScreen.js       # Medication List screen
 │   └── ...
 ├── services/               # API services and data fetching logic
 │   ├── api.js              # API client
 │   └── ...
 ├── styles/                 # Global styles and themes
 │   ├── colors.js           # Color palette
 │   ├── common.js           # Common styles
 │   └── ...
 ├── utils/                  # Utility functions and helpers
 │   ├── dateUtils.js        # Date formatting utilities
 │   └── ...
 ├── .gitignore              # Specifies intentionally untracked files that Git should ignore
 ├── README.md               # Documentation for the project
 └── package.json            # Project dependencies and scripts
```

## 🔐 Android Release Build Setup

When creating a release build for Android:

1.	Rename the example properties file:
    ```bash
    android/gradle.properties.example ➡️ android/gradle.properties
    ```

2.	Update the following values in gradle.properties using credentials from the Validose vault in 1Password:
    ```bash
    VALIDOSE_RELEASE_STORE_FILE=validose-release-key.keystore
    VALIDOSE_RELEASE_KEY_ALIAS=your-alias
    VALIDOSE_RELEASE_STORE_PASSWORD=YOUR_PASSWORD
    VALIDOSE_RELEASE_KEY_PASSWORD=YOUR_PASSWORD
    ```

3.	Copy the keystore file into your project:
    ```bash
    validose-release-key.keystore ➡️ android/app/
    ```

## 🚀 CI/CD (Fastlane + Jenkins)

This repository includes a Fastlane-based CI/CD setup that deploys branch-based environment builds to:

- Google Play (Android)
- TestFlight (iOS)

Jenkins pipeline configuration and credentials mapping are documented in:

- `docs/ci-cd-fastlane-jenkins.md`

Default branch mapping is environment-aware:

- `develop` -> `stage`
- `main` -> `prod`
- feature branches -> `temp` (guarded by Jenkins backdoor parameter)

## Dependencies

Key dependencies used in this project:

- expo
- react
- react-native
- @react-navigation/native
- @react-navigation/stack
- And other dependencies listed in `package.json`

## Contributing

Contributions are welcome! Please follow these steps:

1.  Fork the repository.
2.  Create a new branch for your feature or bug fix.
3.  Make your changes.
4.  Test your changes thoroughly.
5.  Submit a pull request with a clear description of your changes.

## License

This project is licensed under the MIT License - see the `LICENSE` file for details.
