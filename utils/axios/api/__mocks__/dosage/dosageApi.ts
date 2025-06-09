/**
 * Simulates dosage data for a specific device.
 * @param deviceId - The unique identifier for the validose device.
 * @returns Simulated device dosage information.
 */
export const getDeviceDosageSchedule = async (deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  console.log("getDeviceDosageSchedule", deviceId);

  if (deviceId === "A11E919B-4078-D30B-8475-1A7114F99E0B") {
    return {
      regimenId: "R001",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Wednesday", "Thursday"],
      administrationTimesMin: [480, 1200],
      frequencyCount: 2,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 1",
    };
  }

  else {
    return {
      regimenId: "R002",
      indicationCode: "DED",
      dosageAmount: 1,
      administrationDays: ["Monday", "Wednesday", "Thursday"],
      administrationTimesMin: [480, 1200],
      frequencyCount: 3,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 2",
    };
  }
};
