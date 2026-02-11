import { ReactNode } from "react";
import { User } from "../user";

export interface AuthTokenResponse {
  access_token: string;
  refresh_token: string;
}

export interface AuthenticationContextType {
  user: User | null;
  signIn: (userData: AuthTokenResponse) => Promise<void>;
  signOut: () => Promise<void>;
  isLoading: boolean;
  isSignedOut: boolean;
}

export interface AuthenticationProviderProps {
  children: ReactNode;
}
