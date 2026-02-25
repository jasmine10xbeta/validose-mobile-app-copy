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

// Convert to Babel-compatible `process.env` definitions
const envKeys = Object.keys(env).reduce((prev, next) => {
  prev[`process.env.${next}`] = JSON.stringify(env[next]);
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
