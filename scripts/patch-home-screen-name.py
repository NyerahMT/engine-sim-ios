from pathlib import Path

path = Path('.github/workflows/ios-core-manual.yml')
text = path.read_text()

needle = '''          /usr/libexec/PlistBuddy \\
            -c "Set :CFBundleVersion $BUILD_NUMBER" \\
            "$INFO_PLIST"

          echo
          echo "Bundle ID:"
'''

replacement = '''          /usr/libexec/PlistBuddy \\
            -c "Set :CFBundleVersion $BUILD_NUMBER" \\
            "$INFO_PLIST"

          /usr/libexec/PlistBuddy \\
            -c "Set :CFBundleDisplayName Engine Simulator" \\
            "$INFO_PLIST"

          /usr/libexec/PlistBuddy \\
            -c "Set :CFBundleName Engine Simulator" \\
            "$INFO_PLIST"

          echo
          echo "Home Screen display name:"
          /usr/libexec/PlistBuddy \\
            -c "Print :CFBundleDisplayName" \\
            "$INFO_PLIST"

          echo
          echo "Bundle name:"
          /usr/libexec/PlistBuddy \\
            -c "Print :CFBundleName" \\
            "$INFO_PLIST"

          echo
          echo "Bundle ID:"
'''

count = text.count(needle)
if count != 1:
    raise SystemExit(f'Expected one signing identity block, found {count}')

path.write_text(text.replace(needle, replacement, 1))
