export const getBackendProtocol: () => Promise<any> = async () => {
  // Simulate API latency
  return new Promise((resolve) => {
    setTimeout(() => {
      resolve({
        protocol_id: "tp-1234",
        regimen_id: "reg-5678",
        version: 2,
        updated_at: new Date().toISOString(),
        source: "server",
        timezone: "America/New_York",
        medications: [
          {
            name: "Med A",
            dose_times: ["08:00", "20:00"],
            days_of_week: [
              "Monday",
              "Tuesday",
              "Wednesday",
              "Thursday",
              "Friday",
              "Saturday",
              "Sunday",
            ],
          },
          {
            name: "Med B",
            dose_times: ["12:00"],
            days_of_week: [
              "Monday",
              "Tuesday",
              "Wednesday",
              "Thursday",
              "Friday",
              "Saturday",
              "Sunday",
            ],
          },
        ],
      });
    }, 1000);
  });
};
