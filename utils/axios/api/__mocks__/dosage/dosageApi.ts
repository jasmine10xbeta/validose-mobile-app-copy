/**
 * Simulates dosage data for a specific device.
 * @param deviceId - The unique identifier for the validose device.
 * @returns Simulated device dosage information.
 */
export const getDeviceDosageSchedule = async (deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  if (deviceId === "5C8A59AE-59F4-BB24-85F8-39399969232B") {
    return {
      regimen_id: "R001",
      indication_code: "DED",
      dosage_amount: 2,
      administration_days: ["MON", "WED"],
      administration_times_min: [480, 1200],
      frequency_count: 2,
      dosing_window_min: 15,
      active: true,
      notes: "test note 1",
    };
  }

  if (deviceId === "DEVICE-456") {
    return {
      regimen_id: "R002",
      indication_code: "DED",
      dosage_amount: 1,
      administration_days: ["MON", "WED", "THU"],
      administration_times_min: [480, 1200],
      frequency_count: 3,
      dosing_window_min: 15,
      active: true,
      notes: "test note 2",
    };
  }
};
