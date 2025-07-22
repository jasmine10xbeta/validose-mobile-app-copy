import { AuthTokenResponse } from "../axios/api/onboarding";

let signInFn: ((user: AuthTokenResponse) => Promise<void>) | null = null;
let signOutFn: (() => Promise<void>) | null = null;

export function setAuthHandlers(
  signIn: typeof signInFn,
  signOut: typeof signOutFn
) {
  signInFn = signIn;
  signOutFn = signOut;
}

export async function invokeSignIn(user: AuthTokenResponse) {
  if (signInFn) return signInFn(user);
  throw new Error("signIn handler not set");
}

export async function invokeSignOut() {
  if (signOutFn) return signOutFn();
  throw new Error("signOut handler not set");
}