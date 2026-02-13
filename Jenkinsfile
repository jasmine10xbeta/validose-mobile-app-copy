pipeline {
  agent { label 'macos-mobile' }

  options {
    timestamps()
    ansiColor('xterm')
    disableConcurrentBuilds()
    buildDiscarder(logRotator(numToKeepStr: '30', artifactNumToKeepStr: '30'))
  }

  parameters {
    booleanParam(name: 'DEPLOY_ANDROID', defaultValue: true, description: 'Build and deploy Android')
    booleanParam(name: 'DEPLOY_IOS', defaultValue: true, description: 'Build and deploy iOS')
    choice(name: 'DEPLOY_ENV', choices: "auto\nstage\nprod\ntemp", description: 'auto maps by branch: develop->stage, main->prod, other->temp (with backdoor)')
    string(name: 'BACKEND_BASE_URL', defaultValue: '', description: 'Optional override for backend base URL')
    text(name: 'CHANGE_DESCRIPTION', defaultValue: '', description: 'Optional manual release notes appended to commit summary')
    booleanParam(name: 'ALLOW_FEATURE_BRANCH_DEPLOY', defaultValue: false, description: 'Backdoor: allow deploys from non-develop/main branches')
    booleanParam(name: 'UPLOAD_TO_STORES', defaultValue: true, description: 'Upload stage/prod builds to stores')
    booleanParam(name: 'UPLOAD_TEMP_TO_STORES', defaultValue: false, description: 'Upload temp builds from feature branches to stores')
    string(name: 'ANDROID_TRACK', defaultValue: 'internal', description: 'Google Play track for non-prod deployments')
    string(name: 'MATCH_GIT_URL', defaultValue: '', description: 'Optional git URL for fastlane match certificates repo')
  }

  environment {
    CI = 'true'
    LANG = 'en_US.UTF-8'
    LC_ALL = 'en_US.UTF-8'
    BUNDLE_PATH = 'vendor/bundle'
    FASTLANE_SKIP_UPDATE_CHECK = '1'
    FASTLANE_DISABLE_COLORS = '1'

    ANDROID_APPLICATION_ID = 'com.10xbeta.validose'
    IOS_APP_IDENTIFIER = 'com.10xbeta.validose'
    IOS_SCHEME = 'validosemobileapp'
    IOS_WORKSPACE = 'ios/validosemobileapp.xcworkspace'
    MATCH_GIT_URL = "${params.MATCH_GIT_URL}"
    MATCH_READONLY = 'true'

    DEFAULT_STAGE_BASE_URL = 'https://api.stg.aws.validose.com'
    DEFAULT_TEMP_BASE_URL = 'https://api.stg.aws.validose.com'
    DEFAULT_PROD_BASE_URL = 'https://api.aws.validose.com'
    PLAY_STORE_RELEASE_NOTES_LOCALE = 'en-US'
    ANDROID_RELEASE_NOTES_MAX_CHARS = '500'
    IOS_CHANGELOG_MAX_CHARS = '3900'
  }

  stages {
    stage('Checkout') {
      steps {
        checkout scm
      }
    }

    stage('Resolve Deployment Context') {
      steps {
        script {
          def branch = env.BRANCH_NAME?.trim()
          if (!branch) {
            branch = env.GIT_BRANCH?.trim()
          }
          if (!branch || branch == 'HEAD') {
            branch = sh(script: 'git rev-parse --abbrev-ref HEAD', returnStdout: true).trim()
          }
          branch = branch.replaceFirst(/^origin\//, '')

          def isPrimaryBranch = branch in ['develop', 'main']
          def requestedEnv = params.DEPLOY_ENV?.trim() ?: 'auto'
          def resolvedEnv = requestedEnv

          if (requestedEnv == 'auto') {
            if (branch == 'develop') {
              resolvedEnv = 'stage'
            } else if (branch == 'main') {
              resolvedEnv = 'prod'
            } else {
              resolvedEnv = 'temp'
            }
          }

          if (!isPrimaryBranch && !params.ALLOW_FEATURE_BRANCH_DEPLOY) {
            error("Branch '${branch}' is not allowed for deployments unless ALLOW_FEATURE_BRANCH_DEPLOY=true")
          }

          if (resolvedEnv == 'prod' && branch != 'main' && !params.ALLOW_FEATURE_BRANCH_DEPLOY) {
            error("Prod deployments are restricted to main unless ALLOW_FEATURE_BRANCH_DEPLOY=true")
          }

          def defaultBaseUrl = env.DEFAULT_STAGE_BASE_URL
          if (resolvedEnv == 'prod') {
            defaultBaseUrl = env.DEFAULT_PROD_BASE_URL
          } else if (resolvedEnv == 'temp') {
            defaultBaseUrl = env.DEFAULT_TEMP_BASE_URL
          }

          def resolvedBaseUrl = params.BACKEND_BASE_URL?.trim()
          if (!resolvedBaseUrl) {
            resolvedBaseUrl = defaultBaseUrl
          }

          if (!resolvedBaseUrl) {
            error("No backend base URL resolved for env '${resolvedEnv}'. Set BACKEND_BASE_URL or configure defaults in Jenkinsfile.")
          }

          def branchSlug = branch.replaceAll('[^A-Za-z0-9._-]+', '-').replaceAll('^-+|-+$', '')
          if (!branchSlug) {
            branchSlug = 'branch'
          }
          if (branchSlug.length() > 32) {
            branchSlug = branchSlug.take(32)
          }

          def buildLabel = "${resolvedEnv}-${branchSlug}-${env.BUILD_NUMBER}"
          def appVersion = sh(script: "node -p \"require('./package.json').version\"", returnStdout: true).trim()
          def androidVersionName = appVersion
          if (resolvedEnv != 'prod') {
            androidVersionName = "${appVersion}-${resolvedEnv}-${branchSlug}"
            if (androidVersionName.length() > 50) {
              androidVersionName = androidVersionName.take(50)
            }
          }

          def uploadToStores = (resolvedEnv == 'temp') ? params.UPLOAD_TEMP_TO_STORES : params.UPLOAD_TO_STORES
          def previousSuccessfulCommit = env.GIT_PREVIOUS_SUCCESSFUL_COMMIT?.trim()
          def autoReleaseNotes = ''

          if (previousSuccessfulCommit) {
            def previousExists = (sh(script: "git cat-file -e ${previousSuccessfulCommit}^{commit}", returnStatus: true) == 0)
            if (previousExists) {
              autoReleaseNotes = sh(
                script: "git log --max-count=50 --pretty=format:'- %h %s (%an)' ${previousSuccessfulCommit}..HEAD",
                returnStdout: true
              ).trim()
            }
          }

          if (!autoReleaseNotes) {
            autoReleaseNotes = sh(
              script: "git log --max-count=20 --pretty=format:'- %h %s (%an)'",
              returnStdout: true
            ).trim()
          }

          if (!autoReleaseNotes) {
            autoReleaseNotes = '- No commit messages found.'
          }

          def manualReleaseNotes = params.CHANGE_DESCRIPTION?.trim()
          def releaseNotes = manualReleaseNotes
            ? "Manual notes:\n${manualReleaseNotes}\n\nCommit summary:\n${autoReleaseNotes}"
            : "Commit summary:\n${autoReleaseNotes}"

          if (releaseNotes.length() > 8000) {
            releaseNotes = releaseNotes.take(8000) + "\n..."
          }

          env.DEPLOY_ENV_RESOLVED = resolvedEnv
          env.BACKEND_BASE_URL_RESOLVED = resolvedBaseUrl
          env.BUILD_LABEL = buildLabel
          env.RELEASE_LABEL = buildLabel
          env.APP_ENV = resolvedEnv
          env.SHOW_LOGS = (resolvedEnv == 'prod') ? 'false' : 'true'
          env.IOS_BUILD_NUMBER = env.BUILD_NUMBER
          env.ANDROID_VERSION_CODE = env.BUILD_NUMBER
          env.ANDROID_VERSION_NAME = androidVersionName
          env.ANDROID_TRACK = (resolvedEnv == 'prod') ? 'production' : (params.ANDROID_TRACK?.trim() ?: 'internal')
          env.ANDROID_UPLOAD_TO_STORE = uploadToStores.toString()
          env.IOS_UPLOAD_TO_STORE = uploadToStores.toString()
          env.RELEASE_NOTES_FILE = 'build/metadata/release-notes.md'

          currentBuild.displayName = "#${env.BUILD_NUMBER} ${buildLabel}"
          def shortReleaseNotes = releaseNotes.replace("\n", " | ")
          if (shortReleaseNotes.length() > 300) {
            shortReleaseNotes = shortReleaseNotes.take(300) + "..."
          }
          currentBuild.description = "branch=${branch}, env=${resolvedEnv}, base_url=${resolvedBaseUrl}, notes=${shortReleaseNotes}"

          sh 'mkdir -p build/metadata'
          writeFile(file: env.RELEASE_NOTES_FILE, text: releaseNotes + "\n")
        }
      }
    }

    stage('Prepare Build Env File') {
      steps {
        script {
          env.ENV_FILE = "build/env/.env.${env.DEPLOY_ENV_RESOLVED}"
          sh 'mkdir -p build/env'
          writeFile(
            file: env.ENV_FILE,
            text: """BASE_URL=${env.BACKEND_BASE_URL_RESOLVED}
APP_ENV=${env.APP_ENV}
SHOW_LOGS=${env.SHOW_LOGS}
RELEASE_LABEL=${env.RELEASE_LABEL}
"""
          )
        }
      }
    }

    stage('Install Dependencies') {
      steps {
        sh '''
          set -euo pipefail
          npm ci
          if ! command -v bundle >/dev/null 2>&1; then
            gem install bundler -N
          fi
          bundle config set path "$BUNDLE_PATH"
          bundle install --jobs 4 --retry 3
        '''
      }
    }

    stage('Quality Checks') {
      steps {
        sh '''
          set -euo pipefail
          bundle exec fastlane ci_checks
        '''
      }
    }

    stage('Android Build and Upload') {
      when {
        expression { return params.DEPLOY_ANDROID }
      }
      steps {
        script {
          def creds = [
            file(credentialsId: 'validose-android-keystore', variable: 'ANDROID_KEYSTORE_PATH'),
            string(credentialsId: 'validose-android-key-alias', variable: 'VALIDOSE_RELEASE_KEY_ALIAS'),
            string(credentialsId: 'validose-android-store-password', variable: 'VALIDOSE_RELEASE_STORE_PASSWORD'),
            string(credentialsId: 'validose-android-key-password', variable: 'VALIDOSE_RELEASE_KEY_PASSWORD')
          ]
          if (env.ANDROID_UPLOAD_TO_STORE == 'true') {
            creds << file(credentialsId: 'validose-google-play-json', variable: 'PLAY_STORE_JSON_KEY')
          }

          withCredentials(creds) {
            sh '''
              set -euo pipefail
              bundle exec fastlane android develop
            '''
          }
        }
      }
    }

    stage('iOS Build and Upload') {
      when {
        expression { return params.DEPLOY_IOS }
      }
      steps {
        script {
          def creds = []
          if (env.IOS_UPLOAD_TO_STORE == 'true') {
            creds += [
              string(credentialsId: 'validose-appstore-key-id', variable: 'APP_STORE_CONNECT_KEY_ID'),
              string(credentialsId: 'validose-appstore-issuer-id', variable: 'APP_STORE_CONNECT_ISSUER_ID'),
              file(credentialsId: 'validose-appstore-api-key-p8', variable: 'APP_STORE_CONNECT_API_KEY_PATH')
            ]
          }
          if (env.MATCH_GIT_URL?.trim()) {
            creds += [
              string(credentialsId: 'validose-match-git-basic-auth', variable: 'MATCH_GIT_BASIC_AUTHORIZATION'),
              string(credentialsId: 'validose-match-password', variable: 'MATCH_PASSWORD')
            ]
          }

          if (creds.isEmpty()) {
            sh '''
              set -euo pipefail
              bundle exec fastlane ios develop
            '''
          } else {
            withCredentials(creds) {
              sh '''
                set -euo pipefail
                bundle exec fastlane ios develop
              '''
            }
          }
        }
      }
    }
  }

  post {
    always {
      archiveArtifacts artifacts: 'build/**/*.aab,build/**/*.ipa,build/metadata/*.md', allowEmptyArchive: true, fingerprint: true
    }
  }
}
