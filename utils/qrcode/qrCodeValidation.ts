import CryptoJS from "crypto-js";
import { Alert } from "react-native";
import { z } from "zod";

const qrCodeDataSchema = z.object({
  participantId: z.string().uuid(), // Example: Validate as UUID
  doctorId: z.string().uuid(), // Example: Validate as UUID
  deviceId: z.string().min(3), // Example: Minimum length of 3
});

type QRCodeData = z.infer<typeof qrCodeDataSchema>;

export type QRCodeReturnData = {
  qrCode?: QRCodeData;
  validation: boolean;
};

export function qrCodeValidationCheck(data: string): QRCodeReturnData {
  const encryptedQrCode = data;

  try {
    const secretKey = "YourSecretEncryptionKey"; // Get this from a secure config

    //First decrypt the QR code
    //const qrCodeSecretKey = "YourQrCodeSecretKey";
    const bytes = CryptoJS.AES.decrypt(encryptedQrCode, secretKey);
    const decryptedDataString = bytes.toString(CryptoJS.enc.Utf8);

    if (decryptedDataString) {
      try {
        const qrCodeData = JSON.parse(decryptedDataString);

        // Zod Validation
        const validatedData = qrCodeDataSchema.parse(qrCodeData); // Will throw an error if validation fails

        Alert.alert("Success", "QR Code Validated!");
        return { qrCode: validatedData, validation: true };
      } catch (validationError: any) {
        // Explicitly type validationError
        console.error("Zod Validation Error:", validationError);
        Alert.alert(
          "Error",
          `Invalid QR Code Data: ${validationError.message}`
        ); // Display Zod's error message
        return { validation: false };
      }
    } else {
      Alert.alert("Error", "Failed to decrypt QR code.");
      return { validation: false };
    }
  } catch (error) {
    console.error("Error decrypting/parsing QR code:", error);
    Alert.alert("Error", "Invalid QR Code.");
    return { validation: false };
  }
}
