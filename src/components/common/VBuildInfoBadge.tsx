import Constants from "expo-constants";
import { memo } from "react";
import { StyleSheet, Text, View } from "react-native";
import { useSafeAreaInsets } from "react-native-safe-area-context";

const APP_ENV = (process.env.APP_ENV ?? "").trim();
const BASE_URL = (process.env.BASE_URL ?? "").trim().toLowerCase();

function resolveEnvTag(): string {
  if (APP_ENV) {
    return APP_ENV.toUpperCase();
  }

  if (!BASE_URL) {
    return __DEV__ ? "DEVELOPMENT" : "UNKNOWN";
  }

  if (BASE_URL.includes("localhost") || BASE_URL.includes("127.0.0.1")) {
    return "LOCAL";
  }
  if (BASE_URL.includes("stg") || BASE_URL.includes("stage")) {
    return "STAGING";
  }
  if (BASE_URL.includes("dev")) {
    return "DEVELOPMENT";
  }
  if (BASE_URL.includes("prod")) {
    return "PRODUCTION";
  }

  return "UNKNOWN";
}

function resolveVersionAndBuild(): string {
  const appVersion =
    Constants.expoConfig?.version?.trim() || Constants.nativeAppVersion?.trim() || "?.?.?";
  const buildNumber =
    Constants.nativeBuildVersion?.trim() ||
    Constants.expoConfig?.ios?.buildNumber?.trim() ||
    (Constants.expoConfig?.android?.versionCode
      ? String(Constants.expoConfig.android.versionCode)
      : "?");

  return `v${appVersion} (${buildNumber})`;
}

const ENV_TAG = resolveEnvTag();
const VERSION_AND_BUILD = resolveVersionAndBuild();

function VBuildInfoBadgeImpl() {
  const insets = useSafeAreaInsets();

  return (
    <View
      pointerEvents="none"
      style={[
        styles.container,
        {
          bottom: Math.max(insets.bottom, 8),
        },
      ]}
    >
      <View style={styles.badge}>
        <Text style={styles.text}>{`ENV ${ENV_TAG} | ${VERSION_AND_BUILD}`}</Text>
      </View>
    </View>
  );
}

export const VBuildInfoBadge = memo(VBuildInfoBadgeImpl);

const styles = StyleSheet.create({
  container: {
    position: "absolute",
    left: 0,
    right: 0,
    zIndex: 1000,
    alignItems: "center",
  },
  badge: {
    borderRadius: 8,
    paddingHorizontal: 8,
    paddingVertical: 4,
    backgroundColor: "rgba(56, 67, 82, 0.20)",
  },
  text: {
    color: "rgba(255, 255, 255, 0.94)",
    fontSize: 10,
    letterSpacing: 0.3,
    fontWeight: "600",
  },
});
