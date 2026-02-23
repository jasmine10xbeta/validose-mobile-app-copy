import { Feather } from "@expo/vector-icons";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { useCallback, useEffect, useState } from "react";
import {
  Modal,
  StyleProp,
  StyleSheet,
  Text,
  TouchableOpacity,
  TouchableWithoutFeedback,
  View,
  ViewStyle,
} from "react-native";

type VTopActionsProps = {
  onPressHelp: () => void;
  personDisabled?: boolean;
  style?: StyleProp<ViewStyle>;
};

const PATIENT_ID_STORAGE_KEY = "patientId";
const PATIENT_PLACEHOLDER = "Patient ID unavailable";

export function VTopActions({
  onPressHelp,
  personDisabled = false,
  style,
}: VTopActionsProps) {
  const [isPatientCodeOpen, setIsPatientCodeOpen] = useState(false);
  const [patientId, setPatientId] = useState<string | null>(null);
  const [isLoadingPatientId, setIsLoadingPatientId] = useState(false);

  const loadPatientId = useCallback(async () => {
    try {
      setIsLoadingPatientId(true);
      const storedId = await AsyncStorage.getItem(PATIENT_ID_STORAGE_KEY);
      setPatientId(storedId);
    } catch (error) {
      console.warn("[VTopActions] Unable to load patient ID:", error);
      setPatientId(null);
    } finally {
      setIsLoadingPatientId(false);
    }
  }, []);

  useEffect(() => {
    loadPatientId();
  }, [loadPatientId]);

  useEffect(() => {
    if (personDisabled) {
      setIsPatientCodeOpen(false);
    }
  }, [personDisabled]);

  const handlePersonPress = () => {
    if (personDisabled) return;
    loadPatientId();
    setIsPatientCodeOpen(true);
  };

  const displayedPatientId = isLoadingPatientId
    ? "Loading..."
    : patientId || PATIENT_PLACEHOLDER;

  return (
    <View style={[styles.container, style]}>
      <TouchableOpacity
        activeOpacity={0.8}
        style={styles.circleButton}
        disabled={personDisabled}
        accessibilityRole="button"
        accessibilityLabel="Show patient code"
        onPress={handlePersonPress}
      >
        <Feather
          name="user"
          size={24}
          color={personDisabled ? "#A3ADB8" : "#255F6C"}
        />
      </TouchableOpacity>

      <TouchableOpacity
        activeOpacity={0.8}
        style={styles.circleButton}
        accessibilityRole="button"
        accessibilityLabel="View LED info"
        onPress={onPressHelp}
      >
        <Feather name="help-circle" size={26} color="#252F3B" />
      </TouchableOpacity>

      <Modal
        transparent
        animationType="slide"
        visible={isPatientCodeOpen}
        onRequestClose={() => setIsPatientCodeOpen(false)}
      >
        <View style={styles.modalRoot}>
          <TouchableWithoutFeedback onPress={() => setIsPatientCodeOpen(false)}>
            <View style={styles.backdrop} />
          </TouchableWithoutFeedback>
          <View style={styles.sheet}>
            <View style={styles.sheetHeader}>
              <TouchableOpacity
                onPress={() => setIsPatientCodeOpen(false)}
                accessibilityRole="button"
                accessibilityLabel="Close patient code"
                style={styles.closeButton}
              >
                <Text style={styles.closeText}>Close</Text>
              </TouchableOpacity>
              <Text style={styles.sheetTitle}>Patient Code</Text>
              <View style={styles.headerSpacer} />
            </View>

            <Text style={styles.patientCodeText}>{displayedPatientId}</Text>
          </View>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flexDirection: "row",
    width: "100%",
    justifyContent: "space-between",
    alignItems: "center",
    paddingBottom: 24,
  },
  circleButton: {
    width: 48,
    height: 48,
    alignItems: "center",
    justifyContent: "center",
  },
  modalRoot: {
    flex: 1,
    justifyContent: "flex-end",
  },
  backdrop: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: "rgba(0, 0, 0, 0.1)",
  },
  sheet: {
    backgroundColor: "#F4F5F6",
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    paddingTop: 14,
    paddingHorizontal: 16,
    paddingBottom: 28,
    minHeight: 186,
  },
  sheetHeader: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  closeButton: {
    minWidth: 52,
  },
  closeText: {
    fontSize: 15,
    color: "#4D5A69",
    fontWeight: "500",
  },
  sheetTitle: {
    fontSize: 17,
    color: "#2B3645",
    fontWeight: "700",
  },
  headerSpacer: {
    width: 52,
  },
  patientCodeText: {
    marginTop: 30,
    textAlign: "center",
    color: "#2B3645",
    fontSize: 42,
    fontWeight: "700",
  },
});
