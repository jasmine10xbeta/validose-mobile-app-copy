import { signIn, fetchAuthSession } from "@aws-amplify/auth";
import NetInfo from "@react-native-community/netinfo";
import * as SecureStore from "expo-secure-store";

async function waitForInternet(): Promise<void> {
  return new Promise((resolve) => {
    NetInfo.fetch().then((state) => {
      if (state.isConnected) {
        resolve();
      } else {
        const unsubscribe = NetInfo.addEventListener((stateUpdate) => {
          if (stateUpdate.isConnected && stateUpdate.isInternetReachable) {
            unsubscribe();
            resolve();
          }
        });
        
        setTimeout(() => {
          unsubscribe();
          resolve();
        }, 15000);
      }
    });
  });
}

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

    if (!signInRes) return false;
    return true;
  } catch (err: any) {
    if (err.name === "UserAlreadyAuthenticatedException") return true;
    return false;
  }
}

export async function getIdTokenFromSession(): Promise<string | null> {
  try {
    // TODO: Update this logic based on where offline data is stored
    await waitForInternet();
    const session = await fetchAuthSession();
    return session?.tokens?.idToken?.toString() || null;
  } catch (err) {
    console.log("Failed to fetch session token:", err);
    return null;
  }
}

async function ensureValidSession(): Promise<{
  token: string | null;
  success: boolean;
}> {
  const idToken = await getIdTokenFromSession();

  if (idToken) {
    return { token: idToken, success: true };
  }

  try {
    const userAuthDetails = await SecureStore.getItemAsync("authUser");
    if (!userAuthDetails) return { token: null, success: false };

    const { userId, email, password } = JSON.parse(userAuthDetails);
    if (!userId || !email || !password) return { token: null, success: false };

    const signedIn = await signInUser(userId, email, password);
    if (!signedIn) return { token: null, success: false };

    const refreshedToken = await getIdTokenFromSession();
    return { token: refreshedToken, success: !!refreshedToken };
  } catch (error) {
    console.error("Silent re-auth failed:", error);
    return { token: null, success: false };
  }
}

export async function checkSessionValidity(): Promise<boolean> {
  const { success } = await ensureValidSession();
  return success;
}

export async function getValidToken(): Promise<string | null> {
  const { token, success } = await ensureValidSession();
  return success ? token : "";
}
