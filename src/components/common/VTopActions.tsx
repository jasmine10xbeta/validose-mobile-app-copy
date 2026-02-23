import { Feather } from "@expo/vector-icons";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { useCallback, useEffect, useState } from "react";
import {
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
  style?: StyleProp<ViewStyle>;
};

const PATIENT_ID_STORAGE_KEY = "patientId";
const PATIENT_PLACEHOLDER = "Patient ID unavailable";

export function VTopActions({
  onPressHelp,
  personDisabled = false,
  style,
}: VTopActionsProps) {
  const [showPatientInfo, setShowPatientInfo] = useState(false);
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
      setShowPatientInfo(false);
    }
  }, [personDisabled]);

  const handlePersonToggle = () => {
    if (personDisabled) return;
    if (!showPatientInfo) {
      loadPatientId();
    }
    setShowPatientInfo((prev) => !prev);
  };

  const displayedPatientId = isLoadingPatientId
    ? "Loading..."
    : patientId || PATIENT_PLACEHOLDER;

  return (
    <View style={[styles.container, style]}>
      <View style={styles.personRow}>
        {/* <TouchableOpacity
          activeOpacity={0.8}
          style={styles.circleButton}
          disabled={personDisabled}
          accessibilityRole="button"
          accessibilityLabel={
            showPatientInfo ? "Hide patient ID" : "Show patient ID"
          }
          onPress={handlePersonToggle}
        >
          <Feather
            name={showPatientInfo ? "x" : "user"}
            size={24}
            color={personDisabled ? "#A3ADB8" : "#255F6C"}
          />
        </TouchableOpacity> */}
        {showPatientInfo && (
          <View style={styles.patientIdTag}>
            <Text style={styles.patientIdText}>{displayedPatientId}</Text>
          </View>
        )}
      </View>

      {/* <TouchableOpacity
        activeOpacity={0.8}
        style={styles.circleButton}
        accessibilityRole="button"
        accessibilityLabel="View LED info"
        onPress={onPressHelp}
      >
        <Feather name="help-circle" size={26} color="#252F3B" />
      </TouchableOpacity> */}
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
  circleButton: {
    width: 48,
    height: 48,
    alignItems: "center",
    justifyContent: "center",
  },
  patientIdTag: {
    paddingHorizontal: 16,
    paddingVertical: 14,
    borderRadius: 100,
    borderWidth: 1,
    backgroundColor: "#252F3B",
  },
  patientIdText: {
    color: "#FFFFFF",
    fontSize: 14,
  },
});
