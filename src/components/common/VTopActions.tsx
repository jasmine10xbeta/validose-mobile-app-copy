import { Feather } from "@expo/vector-icons";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { type ReactNode, useCallback, useEffect, useState } from "react";
import {
  Image,
  StyleProp,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
  ViewStyle,
} from "react-native";

type VTopActionsProps = {
  onPressHelp: () => void;
  personDisabled?: boolean;
  rightAccessory?: ReactNode;
  style?: StyleProp<ViewStyle>;
};

const PATIENT_ID_STORAGE_KEY = "patientId";
const PATIENT_PLACEHOLDER = "Patient ID unavailable";
const PATIENT_ID_ICON = require("../../assets/images/png/patient-id.png");

export function VTopActions({
  onPressHelp,
  personDisabled = false,
  rightAccessory,
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
    if (!isPatientCodeOpen) {
      loadPatientId();
    }
    setIsPatientCodeOpen((prev) => !prev);
  };

  const displayedPatientId = isLoadingPatientId
    ? "Loading..."
    : patientId || PATIENT_PLACEHOLDER;

  return (
    <View style={[styles.container, style]}>
      <View style={styles.personRow}>
        <TouchableOpacity
          activeOpacity={0.8}
          style={styles.circleButton}
          disabled={personDisabled}
          accessibilityRole="button"
          accessibilityLabel={
            isPatientCodeOpen ? "Hide patient ID" : "Show patient ID"
          }
          onPress={handlePersonPress}
        >
          <Image
            source={PATIENT_ID_ICON}
            style={[
              styles.patientIdIcon,
              personDisabled ? styles.patientIdIconDisabled : null,
            ]}
            resizeMode="contain"
          />
        </TouchableOpacity>
        {isPatientCodeOpen && (
          <View style={styles.patientIdTag}>
            <Text style={styles.patientIdText}>{displayedPatientId}</Text>
          </View>
        )}
      </View>

      <View style={styles.rightActions}>
        {rightAccessory}
        <TouchableOpacity
          activeOpacity={0.8}
          style={styles.circleButton}
          accessibilityRole="button"
          accessibilityLabel="View LED info"
          onPress={onPressHelp}
        >
          <Feather name="help-circle" size={26} color="#252F3B" />
        </TouchableOpacity>
      </View>
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
  personRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: 12,
  },
  rightActions: {
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },
  circleButton: {
    width: 48,
    height: 48,
    alignItems: "center",
    justifyContent: "center",
  },
  patientIdIcon: {
    width: 21,
    height: 21,
  },
  patientIdIconDisabled: {
    opacity: 0.45,
  },
  patientIdTag: {
    paddingHorizontal: 16,
    paddingVertical: 14,
    borderRadius: 100,
    borderWidth: 1,
    backgroundColor: "#252F3B",
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
