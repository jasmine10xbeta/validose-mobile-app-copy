/**
 * Simulates dosage data for a specific device.
 * @param deviceId - The unique identifier for the validose device.
 * @returns Simulated device dosage information.
 */
export const getDeviceDosageSchedule = async (deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  console.log("\n");
  console.log("Simulating API call to backend to fetch dosage..");

  let response;

  if (deviceId === "A11E919B-4078-D30B-8475-1A7114F99E0B") {
    response = {
      regimenId: "R001",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Wednesday", "Thursday"],
      administrationTimesMin: [480, 1200, 1800],
      frequencyCount: 2,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 1",
    };
  }

  response = {
    regimenId: "R001",
    indicationCode: "DED",
    dosageAmount: 2,
    administrationDays: ["Monday", "Tuesday", "Wednesday", "Thursday"],
    administrationTimesMin: [480, 960, 1200, 1600],
    frequencyCount: 2,
    dosingWindowMin: 15,
    active: true,
    notes: "test note 2",
  };

  console.log(`Treatment protocol for device ${deviceId}`, response);
  return response;
};
