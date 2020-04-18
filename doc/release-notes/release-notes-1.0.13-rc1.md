Changelog
=========

Ariel Gabizon (1):
      boost::format -> tinyformat

Bruno Arueira (1):
      Removes out bitcoin mention in favor for snowgem

Cory Fields (1):
      httpserver: explicitly detach worker threads

Duke Leto (1):
      Update performance-measurements.sh

Jack Grigg (36):
      Squashed 'src/snark/' content from commit 9ada3f8
      Add libsnark compile flag to not copy DEPINST to PREFIX
      Add Ansible playbook for grind workers
      Add connections in BIP65 and BIP66 tests to the test manager
      Add benchmark for listunspent
      [Test] MiniNode: Implement JSDescription parsing
      [Test] MiniNode: Implement v2 CTransaction parsing
      [Test] MiniNode: Implement Snowgem block parsing
      [Test] MiniNode: Update protocol version and network magics
      [Test] MiniNode: Use Snowgem PoW
      [Test] MiniNode: Fix coinbase creation
      [Test] MiniNode: Coerce OP_PUSHDATA bytearrays to bytes
      [Test] MiniNode: Implement Snowgem coinbase
      Fix BIP65 and BIP66 tests
      Un-indent RPC test output in test runner
      Replace full-test-suite.sh with a new test suite driver script
      Move ensure-no-dot-so-in-depends.py into full_test_suite.py
      Move check-security-hardening.sh into full_test_suite.py
      Add memory benchmark for validatelargetx
      Migrate libsnark test code to Google Test
      Remove test code corresponding to removed code
      Add alt_bn128 to QAP and Merkle tree gadget tests
      Update libsnark LDLIBS
      Add "make check" to libsnark that runs th