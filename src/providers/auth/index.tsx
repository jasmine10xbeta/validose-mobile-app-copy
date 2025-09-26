import AsyncStorage from "@react-native-async-storage/async-storage";
import * as SecureStore from "expo-secure-store";
import { createContext, useState, useEffect, useContext } from "react";

import { showToast } from "@/components/common/VToast";
import {
  AuthTokenResponse,
  AuthenticationContextType,
  AuthenticationProviderProps,
} from "@/types/auth";

// Create a React context for authentication state and actions
const AuthenticationContext = createContext<
  AuthenticationContextType | undefined
>(undefined);


// Hold references to signIn and signOut handlers for external invocation
let signInFn: ((user: AuthTokenResponse) => Promise<void>) | null = null;
let signOutFn: (() => Promise<void>) | null = null;


// Function to set the internal signIn and signOut handlers, called by the provider
export function setAuthHandlers(
  signIn: typeof signInFn,
  signOut: typeof signOutFn
) {
  signInFn = signIn;
  signOutFn = signOut;
}


// Expose signIn function for use outside React components (e.g. native bridge)
export async function invokeSignIn(user: AuthTokenResponse) {
  if (signInFn) return signInFn(user);
  throw new Error("signIn handler not set");
}


// Expose signOut function similarly for external use
export async function invokeSignOut() {
  if (signOutFn) return signOutFn();
  throw new Error("signOut handler not set");
}

/** SecureStore helpers */
export async function getAccessToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("accessToken");
}
export async function getRefreshToken(): Promise<string | null> {
  return await SecureStore.getItemAsync("refreshToken");
}
export async function storeTokens(accessToken: string, refreshToken?: string) {
  if (accessToken) await SecureStore.setItemAsync("accessToken", accessToken);
  if (refreshToken) await SecureStore.setItemAsync("refreshToken", refreshToken);
}

export async function clearTokens() {
  await SecureStore.deleteItemAsync("accessToken");
  await SecureStore.deleteItemAsync("refreshToken");
}

export function AuthenticationProvider({
  children,
}: AuthenticationProviderProps) {
  const [user, setUser] = useState<any | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(true);
  const [isSignedOut, setIsSignedOut] = useState(false);

  const signIn = async (userData: AuthTokenResponse) => {
    try {
      console.log("\n");
      console.log("Signing in..");
      console.log(`Saving token to SecureStore and loading user..`);

      // Save tokens securely for persistent authentication
      await storeTokens(userData.access_token, userData.refresh_token);
      setUser(userData);
      setIsSignedOut(false);
    } catch (error) {
      console.error("Error saving token to SecureStore:", error);
      showToast("error", "Authentication Error", "Failed to save your session");
    }
  };

  const signOut = async () => {
    try {
      console.log("\n");
      console.log("Signing out.. clearing token and user from SecureStore");

      // Remove tokens and clear user state
      await clearTokens();
      setUser(null);
      setIsSignedOut(true);
    } catch (error) {
      console.error("Error deleting token from SecureStore:", error);
      showToast(
        "error",
        "Authentication Error",
        "Failed to log out your session"
      );
    }
  };

  const FIRST_RUN_KEY = 'is_first_run';

  useEffect(() => {
    console.log("\n");
    console.log(`Loading user from SecureStore..`);

    const loadUser = async () => {
      try {

        const hasRun = await AsyncStorage.getItem(FIRST_RUN_KEY);
        if (!hasRun) {
          console.log("First install detected — clearing SecureStore");
          await clearTokens();
          await AsyncStorage.setItem(FIRST_RUN_KEY, 'true');
        }
        
        const access_token = await getAccessToken();
        const refresh_token = await getRefreshToken();

        // If tokens exist, consider user signed in and restore state
        if (access_token && refresh_token) {
          await signIn({ access_token, refresh_token });
        } else {
          console.log("No tokens found in SecureStore.");
        }
      } catch (error) {
        console.log("Error loading user from SecureStore:", error);
      } finally {
        await new Promise((res) => setTimeout(res, 1000));
        setIsLoading(false);
      }
    };

    loadUser();

    // Register current signIn/signOut handlers for external invocations
    setAuthHandlers(signIn, signOut);
  }, []);

  // Context value that will be accessible to any component using useAuth()
  const value: AuthenticationContextType = {
    user,
    signIn,
    signOut,
    isLoading,
    isSignedOut,
  };

  return (
    <AuthenticationContext.Provider value={value}>
      {children}
    </AuthenticationContext.Provider>
  );
}


// Custom hook to access auth context, throws if used outside provider
export const useAuth = () => {
  const context = useContext(AuthenticationContext);
  if (!context) {
    throw new Error("useAuth must be used within an AuthProvider");
  }
  return context;
};
