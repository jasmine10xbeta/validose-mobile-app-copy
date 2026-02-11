const rawValue = process.env.SHOW_LOGS || "";
const SHOW_LOGS = rawValue.trim().replace(/['"]+/g, "") === "true";

// Pretty banner for console visibility
if (SHOW_LOGS) {
  console.log(`\x1b[1m\x1b[36m==== DEBUG LOGS ENABLED ====\x1b[0m`);
} else {
  console.log(`\x1b[1m\x1b[36m==== DEBUG LOGS DISABLED ====\x1b[0m`);

  // Override all console methods
  console.log = () => {};
  console.info = () => {};
  console.debug = () => {};
  console.warn = () => {};
  console.error = () => {};
}

/**
 * Custom cyan-colored log (only visible if SHOW_LOGS is true).
 * Usage: replace `console.log` with `customLog`
 */
// export function customLog(...args: any[]) {
//   if (SHOW_LOGS) {
//     console.log(`\x1b[36m`, ...args, `\x1b[0m`);
//   }
// }

export function customLog(label: string = "", ...args: any[]) {
  if (!SHOW_LOGS) return;

  const cyan = "\x1b[36m"; // Bold cyan
  const reset = "\x1b[0m";

  const prefix = label ? `${cyan} ${label}${reset}` : "";
  console.log(prefix, ...args);
}

// Default export to disable logs on import
export default null;
