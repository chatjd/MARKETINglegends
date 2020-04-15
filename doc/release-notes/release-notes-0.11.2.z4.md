Alex (1):
      add sha256sum support for Mac OS X

Alfie John (1):
      Rename libzerocash to libzcash

Jack Grigg (21):
      Implement mining slow start with a linear ramp
      Update subsidy tests to account for mining slow start
      Update miner tests to account for mining slow start
      Disable mining slow start in regtest mode
      Fix failing miner test
      Add Snowgem revision to version strings
      Bitcoin -> Snowgem in version and help text
      Add Snowgem Developers to CLI copyright notice
      Minor error message tweak
      Refactor StepRow to make optimisation easier
      Cleanups
      Implement index-truncation Equihash optimisation
      Store truncated indices in the same char* as the hash (H/T tromp for the idea!)
      Use template parameters to statically initialise Equihash
      Merge *StepRow XOR and trimming operations
      Use comparator object for sorting StepRows
      Store full indices in the same char* as the hash
      Use fixed-width array for storing hash and indices
      Use optimised Equihash solver for miner and benchmarks
      Fix comment
      Fix nits after review

Nathan Wilcox (1):
      Fix a test name bug so that ``make cov-snowgem`` correctly runs the ``snowgem-gtest`` binary. Fixes #946.

Sean Bowe (14):
      Refactor PRF_gadget to hand responsibility to PRF_addr_a_pk_gadget for creating the '0' argument to the PRF.
      Enforce first four bits are zero for all spending keys and phi.
      Enable binary serializations of proofs and r1cs keys, and make the `CPourTx` proof field fixed-size.
      Reorder fields of CPourTx to reflect the spec.
      Update proving key and tests that depend on tran