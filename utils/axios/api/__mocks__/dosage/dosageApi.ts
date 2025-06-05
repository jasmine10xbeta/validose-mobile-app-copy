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

  if (deviceId === "5C8A59AE-59F4-BB24-85F8-39399969232B") {
    response = {
      regimenId: "R001",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Wednesday", "Thursday"],
      administrationTimesMin: [480, 620, 1800],    // minutes past midnight
      frequencyCount: 2,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 1",
    };
  } else {
    response = {
      regimenId: "R001",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Tuesday", "Wednesday", "Thursday"],
      administrationTimesMin: [610, 660, 1200],   // minutes past midnight
      frequencyCount: 2,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 2",
    };
  }

  console.log(`Treatment protocol for device ${deviceId}`, response);
  return response;
};
