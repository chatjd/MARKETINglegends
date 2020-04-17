Daira Hopwood (22):
      Add link to protocol specification.
      Add tests for IsStandardTx applied to v2 transactions.
      Make v2 transactions standard. This also corrects a rule about admitting large orphan transactions into the mempool, to account for v2-specific fields.
      Changes to build on Alpine Linux.
      Add Tromp's implementation of Equihash solver (as of tromp/equihash commit 690fc5eff453bc0c1ec66b283395c9df87701e93).
      Integrate Tromp solver into miner code and remove its dependency on extra BLAKE2b implementation.
      Minor edits to dnsseed-policy.md.
      Avoid boost::posix_time functions that have potential out-of-bounds read bugs. ref #1459
      Add help for -equihashsolver= option.
      Assert that the Equihash solver is a supported option.
      Repair check-security-hardening.sh.
      Revert "Avoid boost::posix_time functions that have potential out-of-bounds read bugs. ref #1459"
      Fix race condition in rpc-tests/wallet_protectcoinbase.py. closes #1597
      Fix other potential race conditions similar to ref #1597 in RPC tests.
      Update the error message string for tx version too low. ref #1600
      Static assertion that standard and network min tx versions are consistent.
      Update comments in chainparams.cpp.
      Update unit-tests documentation. closes #1530
      Address @str4d's comments on unit-tests doc. ref #1530
      Remove copyright entries for some files we deleted.
      Update license text in README.md. closes #38
      Bump version numbers to 1.0.0-rc2.

David Mercer (4):
      explicitly pass HOST and BUILD to ./configure
      allow both HOST and BUILD to be passed in from the zcutil/build.sh
      pass in both HOST and BUILD to depends system, needed for deterministic builds
      explicitly pass HOST and BUILD to libgmp ./configure

Gregory Maxwell (1):
      Only send one GetAddr response per connection.

Jack Grigg (31):
      Implement MappedShuffle for tracking the permutation of an array
      Implement static method for creating a randomized JSDescription
      Randomize JoinSplits in z_sendmany
      Refactor test code to better test JSDescription::Randomized()
      Remove stale comment
      Rename libbitcoinconsensus to libzcashconsensus
      Rename bitcoin-tx