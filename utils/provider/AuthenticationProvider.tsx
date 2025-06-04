import {
  createContext,
  useState,
  useEffect,
  ReactNode,
  useContext,
} from "react";
import { showToast } from "@/components/common/Toast";
import { refreshSession } from "../axios/api/__mocks__/login/loginApi";
import {
  clearToken,
  getToken,
  isTokenValid,
  storeToken,
} from "../axios/api/__mocks__/token";

interface User {
  token: string;
  refreshToken: object;
}

interface AuthenticationContextType {
  user: User | null;
  signIn: (userData: User) => Promise<void>;
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
    const loadUser = async () => {
      try {
        const token = await getToken();

        if (token && !isTokenValid(token!)) {
          const newSession = await refreshSession(token!);
          if (!newSession) signOut();
          setUser({
            token: newSession.access_token,
            refreshToken: newSession.refresh_token,
          });
        }
      } catch (error) {
        console.log("Error loading user from SecureStore:", error);
      } finally {
        setIsLoading(false);
      }
    };

    loadUser();
  }, []);

  const signIn = async (userData: any) => {
    try {
      await storeToken(userData.token, userData.refresh_token);
      setUser(userData);
    } catch (error) {
      console.error("Error saving token to SecureStore:", error);
      showToast("error", "Authentication Error", "Failed to save your session");
    }
  };

  const signOut = async () => {
    try {
      await clearToken();
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
