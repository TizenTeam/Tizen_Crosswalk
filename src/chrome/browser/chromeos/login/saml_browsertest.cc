// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";

const char kAnotherUserEmail[] = "alice@example.com";
const char kUserEmail[] = "bob@example.com";

const char kRelayState[] = "RelayState";

// FakeSamlIdp serves IdP auth form and the form submission. The form is
// served with the template's RelayState placeholder expanded to the real
// RelayState parameter from request. The form submission redirects back to
// FakeGaia with the same RelayState.
class FakeSamlIdp {
 public:
  FakeSamlIdp();
  ~FakeSamlIdp();

  void SetUp(const std::string& base_path, const GURL& gaia_url);

  void SetLoginHTMLTemplate(const std::string& template_file);
  void SetLoginAuthHTMLTemplate(const std::string& template_file);

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  scoped_ptr<HttpResponse> BuildHTMLResponse(const std::string& html_template,
                                             const std::string& relay_state,
                                             const std::string& next_path);

  base::FilePath html_template_dir_;

  std::string login_path_;
  std::string login_auth_path_;

  std::string login_html_template_;
  std::string login_auth_html_template_;
  GURL gaia_assertion_url_;

  DISALLOW_COPY_AND_ASSIGN(FakeSamlIdp);
};

FakeSamlIdp::FakeSamlIdp() {
}

FakeSamlIdp::~FakeSamlIdp() {
}

void FakeSamlIdp::SetUp(const std::string& base_path, const GURL& gaia_url) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  html_template_dir_ = test_data_dir.Append("login");

  login_path_= base_path;
  login_auth_path_ = base_path + "Auth";
  gaia_assertion_url_ = gaia_url.Resolve("/SSO");
}

void FakeSamlIdp::SetLoginHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(
      html_template_dir_.Append(template_file),
      &login_html_template_));
}

void FakeSamlIdp::SetLoginAuthHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(
      html_template_dir_.Append(template_file),
      &login_auth_html_template_));
}

scoped_ptr<HttpResponse> FakeSamlIdp::HandleRequest(
    const HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();

  if (request_path == login_path_) {
    std::string relay_state;
    net::GetValueForKeyInQuery(request_url, kRelayState, &relay_state);
    return BuildHTMLResponse(login_html_template_,
                             relay_state,
                             login_auth_path_);
  }

  if (request_path != login_auth_path_) {
    // Request not understood.
    return scoped_ptr<HttpResponse>();
  }

  std::string relay_state;
  FakeGaia::GetQueryParameter(request.content, kRelayState, &relay_state);
  GURL redirect_url = gaia_assertion_url_;

  if (!login_auth_html_template_.empty()) {
    return BuildHTMLResponse(login_auth_html_template_,
                             relay_state,
                             redirect_url.spec());
  }

  redirect_url = net::AppendQueryParameter(
      redirect_url, "SAMLResponse", "fake_response");
  redirect_url = net::AppendQueryParameter(
      redirect_url, kRelayState, relay_state);

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return http_response.PassAs<HttpResponse>();
}

scoped_ptr<HttpResponse> FakeSamlIdp::BuildHTMLResponse(
    const std::string& html_template,
    const std::string& relay_state,
    const std::string& next_path) {
  std::string response_html = html_template;
  ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState", relay_state);
  ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return http_response.PassAs<HttpResponse>();
}

}  // namespace

class SamlTest : public InProcessBrowserTest {
 public:
  SamlTest() : saml_load_injected_(false) {}
  virtual ~SamlTest() {}

  virtual void SetUp() OVERRIDE {
    // Start embedded test server here so that we can get server base url
    // and override Gaia urls in SetupCommandLine.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    // Stop IO thread here because no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    embedded_test_server()->StopThread();

    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");

    const GURL& server_url = embedded_test_server()->base_url();

    std::string gaia_host("gaia");
    GURL::Replacements replace_gaia_host;
    replace_gaia_host.SetHostStr(gaia_host);
    gaia_url_ = server_url.ReplaceComponents(replace_gaia_host);

    command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url_.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url_.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    gaia_url_.spec());
    fake_gaia_.Initialize();

    std::string saml_idp_host("saml.idp");
    GURL::Replacements replace_saml_idp_host;
    replace_saml_idp_host.SetHostStr(saml_idp_host);
    GURL saml_idp_url = server_url.ReplaceComponents(replace_saml_idp_host);
    saml_idp_url = saml_idp_url.Resolve("/SAML/SSO");

    fake_saml_idp_.SetUp(saml_idp_url.path(), gaia_url_);
    fake_gaia_.RegisterSamlUser(kAnotherUserEmail, saml_idp_url);
    fake_gaia_.RegisterSamlUser(kUserEmail, saml_idp_url);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    FakeGaia::MergeSessionParams params;
    params.auth_sid_cookie = kTestAuthSIDCookie;
    params.auth_lsid_cookie = kTestAuthLSIDCookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    params.email = kUserEmail;
    fake_gaia_.SetMergeSessionParams(params);

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &FakeSamlIdp::HandleRequest, base::Unretained(&fake_saml_idp_)));

    // Restart the thread as the sandbox host process has already been spawned.
    embedded_test_server()->RestartThreadAndListen();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }
  }

  WebUILoginDisplay* GetLoginDisplay() {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    CHECK(controller);
    return static_cast<WebUILoginDisplay*>(controller->login_display());
  }

  void WaitForSigninScreen() {
    WizardController::SkipPostLoginScreensForTesting();
    WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    CHECK(wizard_controller);
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());

    content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  }

  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    WaitForSigninScreen();

    if (!saml_load_injected_) {
      saml_load_injected_ = true;

      ASSERT_TRUE(content::ExecuteScript(
          GetLoginUI()->GetWebContents(),
          "$('gaia-signin').gaiaAuthHost_.addEventListener('authFlowChange',"
              "function() {"
                "window.domAutomationController.setAutomationId(0);"
                "window.domAutomationController.send("
                    "$('gaia-signin').isSAML() ? 'SamlLoaded' : 'GaiaLoaded');"
              "});"));
    }

    content::DOMMessageQueue message_queue;  // Start observe before SAML.
    GetLoginDisplay()->ShowSigninScreenForCreds(gaia_email, "");

    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"SamlLoaded\"", message);
  }

  void SetSignFormField(const std::string& field_id,
                        const std::string& field_value) {
    std::string js =
        "(function(){"
          "document.getElementById('$FieldId').value = '$FieldValue';"
          "var e = new Event('input');"
          "document.getElementById('$FieldId').dispatchEvent(e);"
        "})();";
    ReplaceSubstringsAfterOffset(&js, 0, "$FieldId", field_id);
    ReplaceSubstringsAfterOffset(&js, 0, "$FieldValue", field_value);
    ExecuteJsInSigninFrame(js);
  }

  void SendConfirmPassword(const std::string& password_to_confirm) {
    std::string js =
        "$('confirm-password-input').value='$Password';"
        "$('confirm-password').onConfirmPassword_();";
    ReplaceSubstringsAfterOffset(&js, 0, "$Password", password_to_confirm);
    ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(), js));
  }

  void JsExpect(const std::string& js) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetLoginUI()->GetWebContents(),
        "window.domAutomationController.send(!!(" + js + "));",
        &result));
    EXPECT_TRUE(result) << js;
  }

  content::WebUI* GetLoginUI() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
  }

  // Executes Js code in the auth iframe hosted by gaia_auth extension.
  void ExecuteJsInSigninFrame(const std::string& js) {
    ASSERT_TRUE(content::ExecuteScriptInFrame(
        GetLoginUI()->GetWebContents(),
        "//iframe[@id='signin-frame']\n//iframe",
        js));
  }

  FakeSamlIdp* fake_saml_idp() { return &fake_saml_idp_; }

 private:
  GURL gaia_url_;
  FakeGaia fake_gaia_;
  FakeSamlIdp fake_saml_idp_;

  bool saml_load_injected_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should have 'saml' class and 'cancel' button is
// visible when SAML IdP page is loaded. And 'cancel' button goes back to
// gaia on clicking.
IN_PROC_BROWSER_TEST_F(SamlTest, SamlUI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kUserEmail);

  // Saml flow UI expectations.
  JsExpect("$('gaia-signin').classList.contains('saml')");
  JsExpect("!$('cancel-add-user-button').hidden");

  // Click on 'cancel'.
  content::DOMMessageQueue message_queue;  // Observe before 'cancel'.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('cancel-add-user-button').click();"));

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  JsExpect("!$('gaia-signin').classList.contains('saml')");
}

// Tests the sign-in flow when the credentials passing API is used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(kUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Login should finish login and a session should start.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedSingle) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen.
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Enter an unknown password should go back to confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Enter a known password should finish login and start session.
  SendConfirmPassword("fake_password");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the multiple password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedMultiple) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kUserEmail);

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  SetSignFormField("Password1", "password1");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kUserEmail);

  SetSignFormField("Email", "fake_user");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_MESSAGE_BOX).Wait();
  JsExpect(
      "$('message-box-title').textContent == "
      "loadTimeData.getString('noPasswordWarningTitle')");
}

// Types |alice@example.com| into the GAIA login form but then authenticates as
// |bob@example.com| via SAML. Verifies that the logged-in user is correctly
// identified as Bob.
IN_PROC_BROWSER_TEST_F(SamlTest, UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type |alice@example.com| into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(kAnotherUserEmail);

  // Authenticate as bob@example.com via SAML (the |Email| provided here is
  // irrelevant - the authenticated user's e-mail address that FakeGAIA
  // reports was set via SetMergeSessionParams()).
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  SendConfirmPassword("fake_password");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
  const User* user = UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(kUserEmail, user->email());
}


}  // namespace chromeos
