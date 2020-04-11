bitcoind version 0.4.6 is now available for download at:
Windows: installer | zip (sig)
Source: tar.gz
bitcoind and Bitcoin-Qt version 0.6.0.7 are also tagged in git, but it is recommended to upgrade to 0.6.1.

These are bugfix-only releases.

Please report bugs by replying to this forum thread. Note that the 0.4.x wxBitcoin GUI client is no longer maintained nor supported. If someone would like to step up to maintain this, they should contact Luke-Jr.

BUG FIXES

Version 0.6.0 allowed importing invalid "private keys", which would be unspendable; 0.6.0.7 will now verify the private key is valid, and refuse to