import { signIn } from "@aws-amplify/auth";

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
