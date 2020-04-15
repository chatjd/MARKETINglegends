Bitcoin Core version 0.9.0 is now available from:

  https://bitcoin.org/bin/0.9.0/

This is a new major version release, bringing both new features and
bug fixes.

Please report bugs using the issue tracker at github:

  https://github.com/bitcoin/bitcoin/issues

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), uninstall all
earlier versions of Bitcoin, then run the installer (on Windows) or just copy
over /Applications/Bitcoin-Qt (on Mac) or bitcoind/bitcoin-qt (on Linux).

If you are upgrading from version 0.7.2 or earlier, the first time you run
0.9.0 your blockchain files will be re-indexed, which will take anywhere from 
30 minutes to several hours, depending on the speed of your machine.

On Windows, do not forget to uninstall all earlier versions of the Bitcoin
client first, especially if you are switching to the 64-bit version.

Windows 64-bit installer
-------------------------

New in 0.9.0 is the Windows 64-bit version of the client. There have been
frequent reports of users running out of virtual memory on 32-bit systems
during the initial sync. Because of this it is recommended to install the
64-bit version if your system supports it.

NOTE: Release candidate 2 Windows binaries are not code-signed; use PGP
and the SHA256SUMS.asc file to make sure your binaries are correct.
In the final 0.9.0 release, Windows setup.exe binaries will be code-signed.

OSX 10.5 / 32-bit no longer supported
-------------------------------------

0.9.0 drops support for older Macs. The minimum requirements are now:
* A 64-bit-capable CPU (see http://support.apple.com/kb/ht3696);
* Mac OS 10.6 or later (see https://support.apple.com/kb/ht1633).

Downgrading warnings
--------------------

The 'chainstate' for this release is not always compatible with previous
releases, so if you run 0.9 and then decide to switch back to a
0.8.x release you might get a blockchain validation error when starting the
old release (due to 'pruned outputs' being omitted from the index of
unspent transaction outputs).

Running the old release with the -reindex option will rebuild the chainstate
data structures and correct the problem.

Also, the first time you run a 0.8.x release on a 0.9 wallet it will rescan
the blockchain for missing spent coins, which will take a long time (tens
of minutes on a typical machine).

Rebranding to Bitcoin Core
---------------------------

To reduce confusion between Bitcoin-the-network and Bitcoin-the-software we
have renamed the reference client to Bitcoin Core.


OP_RETURN and data in the block chain
-------------------------------------
On OP_RETURN:  There was been some confusion and misunderstanding in
the community, regarding the OP_RETURN feature in 0.9 and data in the
blockchain.  This change is not an endorsement of storing data in the
blockchain.  The OP_RETURN change creates a provably-prunable output,
to avoid data storage schemes -- some of which were already deployed --
that were storing arbitrary data such as images as forever-unspendable
TX outputs, bloating bitcoin's UTXO database.

Storing arbitrary data in the blockchain is still a bad idea; it is less
costly and far more efficient to store non-currency data elsewhere.

Autotools build system
-----------------------

For 0.9.0 we switched to an autotools-based build system instead of individual
(q)makefiles.

Using the standard "./autogen.sh; ./configure; make" to build Bitcoin-Qt and
bitcoind makes it easier for experienced open source developers to contribute 
to the project.

Be sure to check doc/build-*.md for your platform before building from source.

Bitcoin-cli
-------------

Another change in the 0.9 release is moving away from the bitcoind executable
functioning both as a server and as a RPC client. The RPC client functionality
("tell the running bitcoin daemon to do THIS") was split into a separate
executable, 'bitcoin-cli'. The RPC client code will eventually be removed from
bitcoind, but will be kept for backwards compatibility for a release or two.

`walletpassphrase` RPC
-----------------------

The behavior of the `walletpassphrase` RPC when the wallet is already unlocked
has changed between 0.8 and 0.9.

The 0.8 behavior of `walletpassphrase` is to fail when the wallet is already unlocked:

    > walletpassphrase 1000
    walletunlocktime = now + 1000
    > walletpassphrase 10
    Error: Wallet is already unlocked (old unlock time stays)

The new behavior of `walletpassphrase` is to set a new unlock time overriding
the old one:

    > walletpassphrase 1000
    walletunlocktime = now + 1000
    > walletpassphrase 10
    walletunlocktime = now + 10 (overriding the old unlock time)

Transaction malleability-related fixes
--------------------------------------

This release contains a few fixes for transaction ID (TXID) malleability 
issues:

- -nospendzeroconfchange command-line option, to avoid spending
  zero-confirmation change
- IsStandard() transaction rules tightened to prevent relaying and mining of
  mutated transactions
- Additional information in listtransactions/gettransaction output to
  report wallet transactions that conflict with each other because
  they spend the same outputs.
- Bug fixes to the getbalance/listaccounts RPC commands, which would report
  incorrect balances for double-spent (or mutated) transactions.
- New option: -zapwallettxes to rebuild the wallet's transaction information

Transaction Fees
----------------

This release drops the default fee required to relay transactions across the
network and for miners to consider the transaction in their blocks to
0.01mBTC per kilobyte.

Note that getting a transaction relayed across the network does NOT guarantee
that the transaction will be accepted by a miner; by default, miners fill
their blocks with 50 k