const userPoolClientId = process.env.API_AWS_USER_POOLS_WEB_CLIENT_ID?.replace(/^"|"$/g, '');
const userPoolId = process.env.API_AWS_USER_POOLS_ID?.replace(/^"|"$/g, '');

const awsExports = {
  Auth: {
    Cognito: {
      userPoolClientId: userPoolClientId!,
      userPoolId: userPoolId!,
    },
  },
};

export default awsExports;
