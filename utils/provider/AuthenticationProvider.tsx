import * as SecureStore from "expo-secure-store";
import {
  createContext,
  useState,
  useEffect,
  ReactNode,
  useContext,
} from "react";
import { showToast } from "@/components/common/Toast";

interface User {
  token: string;
  // TODO: Add other user properties
}

interface AuthenticationContextType {
  user: User | null;
  signIn: (userData: User) => Promise<void>;
  signOut: () => Promise<void>;
  isLoading: boolean;
}

const AuthenticationContext = createContext<
  AuthenticationContextType | undefined
>(undefined);

interface AuthenticationProviderProps {
  children: ReactNode;
}

const AUTH_TOKEN_KEY = "authToken"; // Define a key for the token

export function AuthenticationProvider({
  children,
}: AuthenticationProviderProps) {
  const [user, setUser] = useState<any | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(true);

  useEffect(() => {
    const loadUser = async () => {
      try {
        const token = await SecureStore.getItemAsync(AUTH_TOKEN_KEY);
        if (token) {
          try {
            // TODO: Add token validation 
            // const isValid = await validateToken(token);
            const isValid = true;
            if (isValid) {
              setUser({ token });
            } else {
              console.log("Invalid token found in storage, signing out");
              await SecureStore.deleteItemAsync(AUTH_TOKEN_KEY);
            }
          } catch (validationError) {
            console.error("Token validation failed:", validationError);
            await SecureStore.deleteItemAsync(AUTH_TOKEN_KEY);
          }
        }
      } catch (error) {
        console.log("Error loading user from SecureStore:", error);
        showToast(
          "error",
          "Authentication Error",
          "Failed to restore your session"
        );
      } finally {
        setIsLoading(false);
      }
    };

    loadUser();
  }, []);

  const signIn = async (userData: any) => {
    try {
      // Assuming userData contains the token
      await SecureStore.setItemAsync(AUTH_TOKEN_KEY, userData.token);
      setUser(userData); // Or decode the token and set user details
    } catch (error) {
      console.error("Error saving token to SecureStore:", error);
      showToast("error", "Authentication Error", "Failed to save your session");
    }
  };

  const signOut = async () => {
    try {
      await SecureStore.deleteItemAsync(AUTH_TOKEN_KEY);
      setUser(null);
    } catch (error) {
      console.error("Error deleting token from SecureStore:", error);
    }
  };

  const value: AuthenticationContextType = { user, signIn, signOut, isLoading };

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
