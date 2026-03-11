const fs = require("fs");
const path = require("path");
const dotenv = require("dotenv");

const envFilePath = process.env.ENV_FILE
  ? path.resolve(process.env.ENV_FILE)
  : path.resolve(".env");

let env = {};
if (fs.existsSync(envFilePath)) {
  // Read and parse env file used for compile-time replacements
  env = dotenv.parse(fs.readFileSync(envFilePath));
}

// Allow CI/EAS to provide values via process.env even when no ENV_FILE exists.
const compileTimeVars = [
  "APP_ENV",
  "BASE_URL",
  "API_AWS_PROJECT_REGION",
  "API_AWS_USER_POOLS_ID",
  "API_AWS_USER_POOLS_WEB_CLIENT_ID",
  "SHOW_LOGS",
  "DEVICE_ID",
  "ENABLE_BLE_BYPASS",
  "BLE_BYPASS_MODE",
  "BLE_BYPASS_KEY",
  "EXPO_PUBLIC_ENABLE_BLE_BYPASS",
  "EXPO_PUBLIC_BLE_BYPASS_MODE",
  "EXPO_PUBLIC_BLE_BYPASS_KEY",
];

const processEnvVars = compileTimeVars.reduce((prev, key) => {
  if (process.env[key] !== undefined) {
    prev[key] = process.env[key];
  }
  return prev;
}, {});

const mergedEnv = {
  ...env,
  ...processEnvVars,
};

// Convert to Babel-compatible `process.env` definitions
const envKeys = Object.keys(mergedEnv).reduce((prev, next) => {
  prev[`process.env.${next}`] = JSON.stringify(mergedEnv[next]);
  return prev;
}, {});

module.exports = function (api) {
  api.cache(true);
  return {
    presets: ["babel-preset-expo"],
    plugins: [
      ["transform-define", envKeys],
      [
        "module-resolver",
        {
          root: ["./"],
          alias: {
            "@": "./src",
          },
          extensions: [".ts", ".tsx", ".js", ".jsx", ".json"],
        },
      ],
    ],
  };
};
