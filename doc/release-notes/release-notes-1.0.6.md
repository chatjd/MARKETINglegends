Cory Fields (4):
      Depends: Add ZeroMQ package
      travis: install a recent libzmq and pyzmq for tests
      build: Make use of ZMQ_CFLAGS
      build: match upstream build change

Daira Hopwood (2):
      Better error reporting for the !ENABLE_WALLET && ENABLE_MINING case.
      Address @str4d's comment about the case where -gen is not set. Also avoid shadowing mineToLocalWallet variable.

Daniel Cousens (3):
      init: amend ZMQ flag names
      init: add zmq to debug categories
      zmq: prepend zmq to debug messages

Jack Grigg (33):
      Update comment
      Remove OpenSSL PRNG reseeding
      Address review comments
      Fix linking error in CreateJoinSplit
      Add compile flag to disable compilation of mining code
      Upgrade OpenSSL to 1.1.0d
      Show all JoinSplit components in getrawtransaction and decoderawtransaction
      Use a more specific exception class for note decryption failure
      Switch miner to P2PKH, add -mineraddress option
      Update help text for mining options
      Correct #ifdef nesting of miner headers and helper functions
      Add ZMQ libs to snowgem-gtest
      Fix python syntax in ZMQ RPC test
      [qa] py2: Unfiddle strings into bytes explicitly in ZMQ RPC test
      Bitcoin -> Snowgem in ZMQ docs
      Add ZeroMQ license to contrib/debian/copyright
      [depends] ZeroMQ 4.2.1
      Clarify that user only needs libzmq if not using depends system
      Bump suggested ZMQ Debian package to 4.1 series
      Add -minetolocalwallet flag, enforced on -mineraddress
      Add test to check for presence of vpub_old & vpub_new in getrawtransaction
      Add a flag for enabling experimental features
      Require -experimentalmode for wallet encryption
      Migrate Snowgem-specific code to UniValue
      Manually iterate over UniValue arrays in tests
      Remove JSON Spirit from contrib/debian/copyright
      unsigned int -> size_t for comparing with UniValue.size()
      [cleanup] Remove unused import
      [cleanup] Simplify test code
      Squashed 'src/univalue/' content from commit 9ef5b78
      Update UniValue includes in Snowgem-specific code
      UniValue::getValues const reference
      Get rid of fPlus argument to FormatMoney in Snowgem-specific code

Jeff Garzik (4):
      Add ZeroMQ support. Notify blocks and transactions via ZeroMQ
      UniValue: prefer .size() to .count(), to harmonize w/ existing tree
      UniValue: export NullUniValue global constant
      Convert tree to using univalue. Eliminate all json_spirit uses.

Johnathan Corgan (5):
      zmq: require version 4.x or newer of libzmq
      zmq: update and cleanup build-unix, release-notes, and zmq docs
      autotoo