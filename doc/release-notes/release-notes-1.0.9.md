Amgad Abdelhafez (2):
      Update timedata.cpp
      Update timedata.cpp

Daira Hopwood (4):
      Fix an error reporting bug due to BrokenPipeError and ConnectionResetError not existing in Python 2. refs #2263
      Alert 1002 (versions 1.0.0-1.0.2 inclusive).
      Alert 1003 (versions 1.0.3-1.0.8 inclusive).
      Disable building Proton by default.

Jack Grigg (14):
      Fix prioritisetransaction RPC test
      torcontrol: Handle escapes in Tor QuotedStrings
      torcontrol: Add missing copyright header
      Convert Snowgem versions to Debian format
      [manpage] Handle build numbers in versions
      Address Daira's comments
      Address Daira's further comments
      Correctly handle three-digit octals with leading digit 4-7
      Check that >3-digit octals are truncated.
      Implement automatic shutdown of deprecated Snowgem versions
      Wrap messages nicely on metrics screen
      Regenerate miner tests
      Add a benchmark for calling ConnectBlock on a block with many inputs
      Remove additional sources of determinism from benchmark archive

Jay Graber (2):
      Change help text examples to use Snowgem addresses
      Poll on getblocktemplate result rather than use bare sleep to avoid race condition.

Nathan Wilcox (39):
      [Direct master commit] Fix a release snafu in debian version string.
      Show toolchain versions in build.sh.
      Start on a make-rele