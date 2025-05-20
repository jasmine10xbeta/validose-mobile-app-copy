import { signIn, fetchAuthSession } from "@aws-amplify/auth";
import AsyncStorage from "@react-native-async-storage/async-storage";

export async function signInUser(
  username: string,
  email: string,
  password: string
): Promise<boolean> {
  try {
    const signInRes = await signIn({
      username,
      password,
      options: { autoSignIn: true, userAttributes: { email } },
    });
    console.log("Cognito sign-in response:", signInRes);

    if (!signInRes) {
      return false;
    }

    return true;
  } catch (err: any) {
    if (err.name === "UserAlreadyAuthenticatedException") {
      console.log("User already signed in.");
      return true;
    }
    console.error("Error during sign-in:", err);
    return false;
  }
}

export async function checkSessionValidity(): Promise<boolean> {
  try {
    const session = await fetchAuthSession();
    const idToken = session?.tokens?.idToken?.toString();

    if (idToken) {
      console.log("Session token exists and session is valid");
      return true;
    }

    console.log("Token missing - trying re-login");
    const authUserString = await AsyncStorage.getItem("authUser");
    if (authUserString) {
      const { userId, email, password } = JSON.parse(authUserString);
      if (!userId || !email || !password) {
        return signInUser(userId, email, password);
      }
    }

    return false;
  } catch (error) {
    console.log("Error checking session and login:", error);
    return false;
  }
}
