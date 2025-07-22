import { DoseRecord } from "@/store/useDoseHistoryStore";

const DEVICE_ID_1 = process.env.DEVICE_ID;

/**
 * Simulates dosage data for a specific device.
 * @param deviceId - The unique identifier for the validose device.
 * @returns Simulated device dosage information.
 */
export const getLatestTreatmentProtocol = async (deviceId: string) => {
  await new Promise((res) => setTimeout(res, 500)); // Simulate delay

  console.log("\n");
  console.log("Simulating API call to backend to fetch dosage..");

  let response;

  if (deviceId === DEVICE_ID_1) {
    response = {
      protocolId: "P001",
      regimenId: "R001",
      deviceId: DEVICE_ID_1,
      medicine: "MED_1",
      medicationName: "MED_1",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday"],
      // administrationTimesMin: [180, 480, 649, 850],    // minutes past midnight
      administrationTimesMin: [
        1752135600, // 03:00
        1752153600, // 08:00
        1752161340, // 10:49
        1752172200  // 14:10
      ],   // epoch time
      frequencyCount: 2,
      dosingWindowMin: 15,
      active: true,
      notes: "test note 1",
    };
  } else {
    response = {
      protocolId: "P002",
      regimenId: "R002",
      deviceId: "DEVICE_ID_2",
      medicine: "MED_2",
      medicationName: "MED_2",
      indicationCode: "DED",
      dosageAmount: 2,
      administrationDays: ["Monday", "Wednesday", "Thursday"],
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

/**
 * Sends an array of dose records to the backend.
 * 
 * @param records - An array of dose records to be sent.
 * @returns A promise that resolves once the records have been sent.
 */
export const sendDoseRecordsToBackend = async (records: DoseRecord[]): Promise<void> => {
  console.log("Sending dose record(s) to backend...", records);
  
  await new Promise((res) => setTimeout(res, 1000));
  // throw new Error("Test failure");
}