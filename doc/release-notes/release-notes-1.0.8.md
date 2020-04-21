Adam Brown (1):
      [doc] Update port in tor.md

Bob McElrath (1):
      Add explicit shared_ptr constructor due to C++11 error

Cory Fields (2):
      libevent: add depends
      libevent: Windows reuseaddr workaround in depends

Daira Hopwood (15):
      Remove src/qt.
      License updates for removal of src/qt.
      Correct license text for LGPL.
      Remove QT gunk from Makefiles.
      Remove some more QT-related stragglers.
      Update documentation for QT removal.
      Update which libraries are allowed to be linked to snowgemd by symbol-check.py.
      Remove NO_QT make option.
      .gitignore cache/ and venv-mnf/
      Remove unused packages and patches.
      Delete -rootcertificates from bash completion script.
      Line-wrap privacy notice. Use <> around URL and end sentence with '.'. Include privacy notice in help text for snowgemd -help.
      Update version numbers.
      Improvement to release process doc.
      Generate man pages.

Daniel Cousens (1):
      torcontrol: only output disconnect if -debug=tor

Gregory Maxwell (3):
      Avoid a compile error on hosts with libevent too old for EVENT_LOG_WARN.
      Do not absolutely protect local peers from eviction.
      Decide eviction group ties based on time.

Ian Kelling (1):
      Docs: add details to -rpcclienttimeout doc

Jack Gavigan (2):
      Removed markdown from COPYING
      Updated the Bitcoin Core copyright statement

Jack Grigg (25):
      Add anchor to output of getblock
      Migrate IncrementalMerkleTree memory usage calls
      Add tests for getmempoolinfo
      Usability improvements for z_importkey
      Implement an AtomicTimer
      Use AtomicTimer for more accurate local solution rate
      Metrics: Move local solution rate into stats
      Metrics: Improve mining status
      Expand on reasons for mining being paused
      Simplify z_importkey by making rescan a string
      Revert "Closes #1680, temporary fix for rpc deadlock inherited from upstream."
      Add libevent to snowgem-gtest
      [depends] libevent 2.1.8
      Test boolean fallback in z_importkey
