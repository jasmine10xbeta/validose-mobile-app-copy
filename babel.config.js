const fs = require("fs");
const dotenv = require("dotenv");

let env = {};
if (fs.existsSync(".env")) {
  // Read and parse .env
  env = dotenv.parse(fs.readFileSync(".env"));
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
    plugins: [["transform-define", envKeys]],
  };
};