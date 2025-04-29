import { useCameraPermissions } from "expo-camera";
import { useRouter } from "expo-router";
import { FlatList, StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VButton } from "@/components/common/VButton";
import { VDeviceItem } from "@/components/common/VDeviceItem";
import { VText } from "@/components/common/VText";
import { useAuthStore } from "@/store/authStore";
import useDeviceStore from "@/store/useDeviceStore";

export default function SetupDeviceConnectScreen() {
  const { setToken } = useAuthStore();
  const router = useRouter();
  const { getDeviceList } = useDeviceStore();
  const [permission, requestPermission] = useCameraPermissions();

  if (!permission) {
    return <View />;
  }

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <View style={styles.setupDeviceConnectContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  function onScanDevicePress() {
    router.push("/QRCodeScanner");
  }

  function onContinuePress() {
    setToken("MyToken");
    router.push("/Dashboard");
  }

  const isDevicesConnected = getDeviceList().length > 0;

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.setupDeviceConnectContainer}>
        <VText textVariant="Body">
          {isDevicesConnected
            ? `Device connection successful`
            : `Let's connect your device`}
        </VText>
        <VButton onPress={onScanDevicePress} label="+ Scan device" />
        {isDevicesConnected ? (
          <VButton onPress={onContinuePress} label="Continue" />
        ) : null}
        <VText textVariant="LabelUnderline" textDecorationStyle="solid">
          Enter device ID manually
        </VText>
        <View style={styles.scrollViewSection}>
          {isDevicesConnected ? (
            <FlatList
              initialNumToRender={4}
              renderItem={({ item }) => (
                <VDeviceItem item={item} state={"Conected"} />
              )}
              keyExtractor={(item) => item.id}
              data={getDeviceList()}
            />
          ) : null}
        </View>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  alignContent: {
    height: "100%",
    justifyContent: "center",
    alignContent: "center",
    backgroundColor: "#FFF",
  },
  setupDeviceConnectContainer: {
    padding: 20,
    flexDirection: "column",
    alignItems: "center",
    gap: 25,
  },
  textInput: {
    height: 50,
    width: 250,
  },
  camera: {
    height: 250,
    width: 250,
  },
  container: {
    flex: 1,
    justifyContent: "center",
  },
  message: {
    textAlign: "center",
    paddingBottom: 10,
  },
  buttonContainer: {
    flexDirection: "row",
  },
  button: {
    flex: 1,
    alignSelf: "flex-end",
    alignItems: "center",
  },
  text: {
    fontSize: 24,
    fontWeight: "bold",
    color: "white",
  },
  scrollViewSection: {
    height: "50%",
  },
});
