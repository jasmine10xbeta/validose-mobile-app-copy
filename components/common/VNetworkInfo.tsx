import { useMemo } from "react";
import { View, Image, StyleSheet } from "react-native";
import useNetworkStore from "@/store/useNetworkStore";
import { VText } from "./VText";

export default function VNetworkInfo() {
  const { isConnected, lastDisconnectedAt, isStaleSync } = useNetworkStore();

  const isStaleConnection = useMemo(() => {
    if (!lastDisconnectedAt) return false;
    const disconnectedDuration = Date.now() - lastDisconnectedAt;
    return disconnectedDuration > 24 * 60 * 60 * 1000; // > 24 hours
  }, [lastDisconnectedAt]);

  const shouldShowBanner = !isConnected || isStaleSync() || isStaleConnection;
  if (!shouldShowBanner) return null;

  const message =
    isStaleConnection || isStaleSync()
      ? "Stale Sync"
      : "No Internet Connection";

  const subtitle =
    isStaleConnection || isStaleSync()
      ? "Warning: Your data may be out of date."
      : "Please check your internet connection.";

  return (
    <View style={styles.banner}>
      <Image
        source={require("./../../assets/images/alert-diamond-red.png")}
        style={styles.image}
      />
      <View style={{ flexDirection: "column", width: "100%" }}>
        <VText textVariant="LabelMedicineBold" marginBottom={2}>
          {message}
        </VText>
        <VText textVariant="LabelMedicine">{subtitle}</VText>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  banner: {
    marginTop: 20,
    width: "100%",
    borderRadius: 8,
    padding: 15,
    gap: 15,
    flexDirection: "row",
    alignItems: "center",
    backgroundColor: "#FC9E9E33",
  },
  image: {
    height: 40,
    width: 40,
  },
});
