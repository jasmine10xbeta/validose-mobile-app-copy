import { useAssets } from "expo-asset";
import { useCameraPermissions } from "expo-camera";
import { Image } from "expo-image";
import { useRouter } from "expo-router";
import { StyleSheet, View } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";
import { VButton } from "@/components/common/VButton";
import { VText } from "@/components/common/VText";
import { validoseWhite } from "@/constants/Colors";
import { useAuthStore } from "@/store/authStore";

export default function LoginScreen() {
  const { doneLogging } = useAuthStore();
  const router = useRouter();

  const [assets] = useAssets([
    require("../assets/images/validose-logo-dark.png"),
  ]);
  const [permission, requestPermission] = useCameraPermissions();

  if (!permission) {
    return <View />;
  }

  if (!permission.granted) {
    return (
      <SafeAreaView style={styles.alignContent}>
        <View style={styles.loginContainer}>
          <VText textVariant="Body">
            We need your permission to show the camera
          </VText>
          <VButton onPress={requestPermission} label="Grant permission" />
        </View>
      </SafeAreaView>
    );
  }

  function onLoginPress() {
    router.push("/Dashboard");
  }

  function onPairingPress() {
    router.push("/Pairing");
  }

  return (
    <SafeAreaView style={styles.alignContent}>
      <View style={styles.loginContainer}>
        {assets ? <Image source={assets[0]} style={styles.image} /> : null}
        <VText textVariant="Label">Clinical Trial Mobile App</VText>
        <View style={styles.buttonContainer}>
          <VButton onPress={onPairingPress} label="Pairing" />
          <VButton
            onPress={onLoginPress}
            label="Login"
            disabled={!doneLogging}
          />
          <VText textVariant="LabelUnderline" textDecorationStyle="solid">
            Need Help with login?
          </VText>
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
  loginContainer: {
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
    width: "100%",
    gap: 20,
    marginTop: 150,
    flexDirection: "column",
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
  image: {
    width: 335,
    height: 70,
    backgroundColor: validoseWhite,
  },
});
