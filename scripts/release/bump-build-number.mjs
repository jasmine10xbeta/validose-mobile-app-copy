#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";

const APP_JSON_PATH = path.resolve(process.cwd(), "app.json");

const androidVersionCodePattern = /("versionCode"\s*:\s*)(\d+)/;
const iosBuildNumberPattern = /("buildNumber"\s*:\s*")(\d+)(")/;

function exitWithUsage(message) {
  if (message) {
    console.error(message);
  }
  console.error(
    "Usage:\n" +
      "  node ./scripts/release/bump-build-number.mjs --show\n" +
      "  node ./scripts/release/bump-build-number.mjs --bump [increment]\n" +
      "  node ./scripts/release/bump-build-number.mjs --set <buildNumber>"
  );
  process.exit(1);
}

function parsePositiveInt(value, label) {
  if (!value || !/^\d+$/.test(value)) {
    exitWithUsage(`Invalid ${label}: ${value ?? "(empty)"}`);
  }

  const parsed = Number.parseInt(value, 10);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    exitWithUsage(`Invalid ${label}: ${value}`);
  }
  return parsed;
}

if (!fs.existsSync(APP_JSON_PATH)) {
  console.error(`app.json not found at ${APP_JSON_PATH}`);
  process.exit(1);
}

const args = process.argv.slice(2);
const command = args[0];
if (!command) {
  exitWithUsage("Missing command.");
}

const appJsonText = fs.readFileSync(APP_JSON_PATH, "utf8");
const androidMatch = appJsonText.match(androidVersionCodePattern);
const iosMatch = appJsonText.match(iosBuildNumberPattern);

if (!androidMatch || !iosMatch) {
  console.error(
    'Could not find both "expo.android.versionCode" and "expo.ios.buildNumber" in app.json.'
  );
  process.exit(1);
}

const androidVersionCode = Number.parseInt(androidMatch[2], 10);
const iosBuildNumber = Number.parseInt(iosMatch[2], 10);
const currentBuild = Math.max(androidVersionCode, iosBuildNumber);

if (!Number.isSafeInteger(androidVersionCode) || !Number.isSafeInteger(iosBuildNumber)) {
  console.error("Could not parse current build numbers from app.json.");
  process.exit(1);
}

if (command === "--show") {
  console.log(
    JSON.stringify(
      {
        androidVersionCode,
        iosBuildNumber,
      },
      null,
      2
    )
  );
  process.exit(0);
}

let nextBuild = currentBuild;
if (command === "--bump") {
  const increment = args[1] ? parsePositiveInt(args[1], "increment") : 1;
  nextBuild = currentBuild + increment;
} else if (command === "--set") {
  nextBuild = parsePositiveInt(args[1], "buildNumber");
} else {
  exitWithUsage(`Unknown command: ${command}`);
}

const updatedText = appJsonText
  .replace(androidVersionCodePattern, `$1${nextBuild}`)
  .replace(iosBuildNumberPattern, `$1${nextBuild}$3`);

fs.writeFileSync(APP_JSON_PATH, updatedText);

console.log(
  `Updated app.json build numbers: android.versionCode ${androidVersionCode} -> ${nextBuild}, ` +
    `ios.buildNumber ${iosBuildNumber} -> ${nextBuild}`
);
