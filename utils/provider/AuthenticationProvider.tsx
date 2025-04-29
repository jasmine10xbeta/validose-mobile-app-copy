import * as SecureStore from "expo-secure-store";
import {
  createContext,
  useState,
  useEffect,
  ReactNode,
  useContext,
} from "react";

interface AuthenticationContextType {
  user: any | null;
  signIn: (userData: any) => Promise<void>;
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
          // Verify the token (e.g., decode it, make an API call)
          // For simplicity, let's assume the token itself is the user data
          setUser({ token }); // Or decode the token and set user details
        }
      } catch (error) {
        console.error("Error loading user from SecureStore:", error);
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
