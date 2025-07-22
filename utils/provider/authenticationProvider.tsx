import {
  createContext,
  useState,
  useEffect,
  ReactNode,
  useContext,
} from "react";
import { showToast } from "@/components/common/VToast";
import { refreshSession } from "../axios/api/__mocks__/login/loginApi";
import {
  clearTokens,
  getToken,
  isTokenValid,
  storeTokens,
} from "../axios/api/__mocks__/token";
import { AuthTokenResponse } from "../axios/api/onboarding";
import { setAuthHandlers } from "./authManager";

interface User {
  token: string;
  refreshToken: object;
}

interface AuthenticationContextType {
  user: User | null;
  signIn: (userData: AuthTokenResponse) => Promise<void>;
  signOut: () => Promise<void>;
  isLoading: boolean;
  isSignedOut: boolean;
}

const AuthenticationContext = createContext<
  AuthenticationContextType | undefined
>(undefined);

interface AuthenticationProviderProps {
  children: ReactNode;
}

export function AuthenticationProvider({
  children,
}: AuthenticationProviderProps) {
  const [user, setUser] = useState<any | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(true);
  const [isSignedOut, setIsSignedOut] = useState(false);

  useEffect(() => {
    console.log("\n");
    console.log(`Loading user from SecureStore..`);

    const loadUser = async () => {
      try {
        const token = await getToken();
        console.log(`Token from SecureStore:\n ${token}`);

        if (token && !isTokenValid(token!)) {
          console.log("Token is not valid..");
          const newSession = await refreshSession(token!);

          if (!newSession) signOut();

          signIn({
            access_token: newSession.access_token,
            refresh_token: newSession.refresh_token,
          });
        }
      } catch (error) {
        console.log("Error loading user from SecureStore:", error);
      } finally {
        await new Promise((res) => setTimeout(res, 1000));
        setIsLoading(false);
      }
    };

    loadUser();

    setAuthHandlers(signIn, signOut);
  }, []);

  const signIn = async (userData: AuthTokenResponse) => {
    try {
      console.log("\n");
      console.log("Signing in..");
      console.log(`Saving token to SecureStore and loading user\n ${JSON.stringify(userData)}`);

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
      
      await clearTokens();
      setUser(null);
      setIsSignedOut(true);
    } catch (error) {
      console.error("Error deleting token from SecureStore:", error);
      showToast("error", "Authentication Error", "Failed to log out your session");
    }
  };

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

export const useAuth = () => {
  const context = useContext(AuthenticationContext);
  if (!context) {
    throw new Error("useAuth must be used within an AuthProvider");
  }
  return context;
};
