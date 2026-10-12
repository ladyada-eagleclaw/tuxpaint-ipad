# Tux Paint for iPhone and iPad

An experimental, ad-free Apple-device build of [Tux Paint](https://tuxpaint.org/),
based on Tux Paint 0.9.31 and the
[happymobilecq iOS port](https://github.com/happymobilecq/tuxpaint_ios).
The goal is a usable family TestFlight build with the familiar brushes, stamps,
shapes, sounds, and magic tools.

## Devices and current limitations

- Universal arm64 app targeting iOS/iPadOS 15 or later, including iPhone,
  iPad mini, and iPad Pro. Use it in landscape orientation.
- Controls scale with the canvas. Rendering and touch coordinates account for
  the iPhone notch, Dynamic Island, and home-indicator safe area.
- Drawings are stored in the app sandbox; Photos export requests add-only access.
- No ads, in-app purchases, account sign-in, or printing.
- SVG stamps are not supported by this inherited port. PNG stamps are bundled.
- This remains a beta. Simulator startup checks do not replace testing drawing,
  saving, reopening, sound, and Photos export on physical devices.
- Recent iPadOS versions may show a landscape canvas with borders while the iPad
  is held upright. This build uses Apple's full-screen compatibility mode;
  adaptive portrait layouts and multitasking are not implemented.

The App Store Connect entry is currently named **Tux Paint iPad Lab**, even
though the build supports both iPhone and iPad. TestFlight installation requires
an invitation; unsigned CI artifacts cannot be installed as TestFlight apps.

## Build without a local Mac

GitHub Actions supplies the Mac and Xcode. In the repository's **Actions** tab,
run **iPad build** against `main`, or use:

```sh
gh workflow run ipad.yml --ref main
```

The workflow builds device and simulator apps with Xcode 26.3, then launches
the simulator app on an iPad Pro 12.9-inch (5th generation), iPad mini
(6th generation), and iPhone 16. It captures startup screenshots and logs,
checks that the app stays running, and publishes unsigned ZIP artifacts for
seven days. The workflow name reflects the project's original iPad target.

## Build locally on a Mac

Install Xcode 26.3 and CMake 3.24 or newer. Select Xcode, then run:

```sh
sudo xcode-select -s /Applications/Xcode_26.3.app/Contents/Developer
cmake -S ipad -B build-ipad -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-ipad --config Release --target TuxPaint --parallel 3
```

Use `iphonesimulator` instead of `iphoneos` for an Apple-silicon simulator build.
Dependencies are fetched at versions pinned in [ipad/CMakeLists.txt](ipad/CMakeLists.txt).
Signing is disabled unless a distribution profile is explicitly configured.

## Upload to TestFlight

The bundle ID is `org.ladyada.tuxpaint.ipad`. The Apple developer team needs a
matching App Store Connect app, an Apple Distribution certificate with its
private key, an App Store provisioning profile, and an App Store Connect API
key permitted to upload builds.

Configure these **GitHub Actions secrets**, never files committed to the repo:

| Secret | Value |
| --- | --- |
| `BUILD_CERTIFICATE_BASE64` | Base64-encoded distribution certificate/private-key `.p12` |
| `P12_PASSWORD` | Password protecting the `.p12` |
| `BUILD_PROVISION_PROFILE_BASE64` | Base64-encoded App Store `.mobileprovision` |
| `ASC_PRIVATE_KEY_BASE64` | Base64-encoded API private key `.p8` |
| `ASC_KEY_ID` | API key ID |
| `ASC_ISSUER_ID` | App Store Connect issuer ID |

The workflow defaults to this repository's developer team. Set the repository
variable `APPLE_TEAM_ID` to override it when adapting the project.

Run **iPad TestFlight upload** against `main`, or:

```sh
gh workflow run testflight.yml --ref main
```

The workflow imports credentials into a temporary keychain, creates a signed
archive, and uploads it to Apple. It does not publish signed artifacts or save
signing credentials in a build cache. Generated credentials are removed from
the runner afterward. An upload is not an invitation: Apple must process the
build, then it must be assigned to a tester group. External testing also requires
Apple's beta review.

The `ipad-cloud-build` and `testflight-upload` branches retain push triggers from
the initial setup. Routine maintenance can commit directly to `main`; pull
requests are not required for this repository.

## Source and licenses

- [ipad/](ipad/) contains the current CMake project, app metadata, Photos bridge,
  export settings, and configuration.
- [tuxpaint-tuxpaint/](tuxpaint-tuxpaint/) contains the inherited Tux Paint source
  and assets. [IOS/](IOS/) retains files from the earlier iOS port; its old Xcode
  project is not the build entry point used here.
- [tuxpaint-stamps-2023.07.20/](tuxpaint-stamps-2023.07.20/) provides bundled stamps.

Tux Paint is licensed under GPL version 2 or later; see
[COPYING.txt](tuxpaint-tuxpaint/docs/COPYING.txt). Keep the original copyright and
license notices. Stamps, fonts, and third-party libraries retain their respective
licenses; see their accompanying files and the
[stamp license](tuxpaint-stamps-2023.07.20/docs/COPYING.txt).
