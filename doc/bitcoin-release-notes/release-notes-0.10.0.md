Bitcoin Core version 0.10.0 is now available from:

  https://bitcoin.org/bin/0.10.0/

This is a new major version release, bringing both new features and
bug fixes.

Please report bugs using the issue tracker at github:

  https://github.com/bitcoin/bitcoin/issues

Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Bitcoin-Qt (on Mac) or
bitcoind/bitcoin-qt (on Linux).

Downgrading warning
---------------------

Because release 0.10.0 makes use of headers-first synchronization and parallel
block download (see further), the block files and databases are not
backwards-compatible with older versions of Bitcoin Core or other software:

* Blocks will be stored on disk out of order (in the order they are
received, really), which makes it incompatible with some tools or
other programs. Reindexing using earlier versions will also not work
anymore as a result of this.

* The block index database will now hold headers for which no block is
stored on disk, which earlier versions won't support.

If you want to be able to downgrade smoothly, make a backup of your entire data
directory. Without this your node will need start syncing (or importing from
bootstrap.dat) anew afterwards. It is possible that the data from a completely
synchronised 0.10 node may be usable in older versions as-is, but this is not
supported and may break as soon as the older version attempts to reindex.

This does not affect wallet forward or backward compatibility.


Notable changes
===============

Faster synchronization
----------------------

Bitcoin Core now uses 'headers-first synchronization'. This means that we first
ask peers for block headers (a total of 27 megabytes, as of December 2014) and
validate those. In a second stage, when the headers have been discovered, we
download the blocks. However, as we already know about the whole chain in
advance, the blocks can be downloaded in parallel from all available peers.

In practice, this means a much faster and more robust synchronization. On
recent hardware with a decent network link, it can be as little as 3 hours
for an initial full synchronization. You may notice a slower progress in the
very first few minutes, when headers are still being fetched and verified, but
it should gain speed afterwards.

A few RPCs were added/updated as a result of this:
- `getblockchaininfo` now returns the number of validated headers in addition to
the number of validated blocks.
- `getpeerinfo` lists both the number of blocks and headers we know we have in
common with each peer. While synchronizing, the heights of the blocks that we
have requested from peers (but haven't received yet) are also listed as
'inflight'.
- A new RPC `getchaintips` lists all known branches of the block chain,
including those we only have headers for.

Transaction fee changes
-----------------------

This release automatically estimates how high a transaction fee (or how
high a priority) transactions require to be confirmed quickly. The default
settings will create transactions that confirm quickly; see the new
'txconfirmtarget' setting to control the tradeoff between fees and
confirmation times. Fees are added by default unless the 'sendfreetransactions' 
setting is enabled.

Prior releases used hard-coded fees (and priorities), and would
sometimes create transactions that took a very long time to confirm.

Statistics used to estimate fees and priorities are saved in the
data directory in the `fee_estimates.dat` file just before
program shutdown, and are read in at startup.

New command line options for transaction fee changes:
- `-txconfirmtarget=n` : create transactions that have enough fees (or priority)
so they are likely to begin confirmation within n blocks (default: 1). This setting
is over-ridden by the -paytxfee option.
- `-sendfreetransactions` : Send transactions as zero-fee transactions if possible 
(default: 0)

New RPC commands for fee estimation:
- `estimatefee nblocks` : Returns approximate fee-per-1,000-bytes needed for
a transaction to begin confirmation within nblocks. Returns -1 if not enough
transactions have been observed to compute a good estimate.
- `estimatepriority nblocks` : Returns approximate priority needed for
a zero-fee transaction to begin confirmation within nblocks. Returns -1 if not
enough free transactions have been observed to compute a good
estimate.

RPC access control changes
--------------------------

Subnet matching for the purpose of access control is now done
by matching the binary network address, instead of with string wildcard matching.
For the user this means that `-rpcallowip` takes a subnet specification, which can be

- a single IP address (e.g. `1.2.3.4` or `fe80::0012:3456:789a:bcde`)
- a network/CIDR (e.g. `1.2.3.0/24` or `fe80::0000/64`)
- a network/netmask (e.g. `1.2.3.4/255.255.255.0` or `fe80::0012:3456:789a:bcde/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff`)

An arbitrary number of `-rpcallow` arguments can be given. An incoming connection will be accepted if its origin address
matches one of them.

For example:

| 0.9.x and before                           | 0.10.x                                |
|--------------------------------------------|---------------------------------------|
| `-rpcallowip=192.168.1.1`                  | `-rpcallowip=192.168.1.1` (unchanged) |
| `-rpcallowip=192.168.1.*`                  | `-rpcallowip=192.168.1.0/24`          |
| `-rpcallowip=192.168.*`                    | `-rpcallowip=192.168.0.0/16`          |
| `-rpcallowip=*` (dangerous!)               | `-rpcallowip=::/0` (still dangerous!) |

Using wildcards will result in the rule being rejected with the following error in debug.log:

    Error: Invalid -rpcallowip subnet specification: *. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).


REST interface
--------------

A new HTTP API is exposed when running with the `-rest` flag, which allows
unauthenticated access to public node data.

It is served on the same port as RPC, but does not need a password, and uses
plain HTTP instead of JSON-RPC.

Assuming a local RPC server running on port 8332, it is possible to request:
- Blocks: http://localhost:8332/rest/block/*HASH*.*EXT*
- Blocks without transactions: http://localhost:8332/rest/block/notxdetails/*HASH*.*EXT*
- Transactions (requires `-txindex`): http://localhost:8332/rest/tx/*HASH*.*EXT*

In every case, *EXT* can be `bin` (for raw binary data), `hex` (for hex-encoded
binary) or `json`.

For more details, see the `doc/REST-interface.md` document in the repository.

RPC Server "Warm-Up" Mode
-------------------------

The RPC server is started earlier now, before most of the expensive
intialisations like loading the block index.  It is available now almost
immediately after starting the process.  However, until all initialisations
are done, it always returns an immediate error with code -28 to all calls.

This new behaviour can be useful for clients to know that a server is already
started and will be available soon (for instance, so that they do not
have to start it themselves).

Improved signing security
-------------------------

For 0.10 the security of signing against unusual attacks has been
improved by making the signatures constant time and deterministic.

This change is a result of switching signing to use libsecp256k1
instead of OpenSSL. Libsecp256k1 is a cryptographic library
optimized for the curve Bitcoin uses which was created by Bitcoin
Core developer Pieter Wuille.

There exist attacks[1] against most ECC implementations where an
attacker on shared virtual machine hardware could extract a private
key if they could cause a target to sign using the same key hundreds
of times. While using shared hosts and reusing keys are inadvisable
for other reasons, it's a better practice to avoid the exposure.

OpenSSL has code in their source repository for derandomization
and reduction in timing leaks that we've eagerly wanted to use for a
long time, but this functionality has still not made its
way into a released version of OpenSSL. Libsecp256k1 achieves
significantly stronger protection: As far as we're aware this is
the only deployed implementation of constant time signing for
the curve Bitcoin uses and we have reason to believe that
libsecp256k1 is better tested and more thoroughly reviewed
than the implementation in OpenSSL.

[1] https://eprint.iacr.org/2014/161.pdf

Watch-only wallet support
-------------------------

The wallet can now track transactions to and from wallets for which you know
all addresses (or scripts), even without the private keys.

This can be used to track payments without needing the private keys online on a
possibly vulnerable system. In addition, it can help for (manual) construction
of multisig transactions where you are only one of the signers.

One new RPC, `importaddress`, is added which functions similarly to
`importprivkey`, but instead takes an address or script (in hexadecimal) as
argument.  After using it, outputs credited to this address or script are
considered to be received, and transactions consuming these outputs will be
considered to be sent.

The following RPCs have optional support for watch-only:
`getbalance`, `listreceivedbyaddress`, `listreceivedbyaccount`,
`listtransactions`, `listaccounts`, `listsinceblock`, `gettransaction`. See the
RPC documentation for those methods for more information.

Compared to using `getrawtransaction`, this mechanism does not require
`-txindex`, scales better, integrates better with the wallet, and is compatible
with future block chain pruning functionality. It does mean that all relevant
addresses need to added to the wallet before the payment, though.

Consensus library
-----------------

Starting from 0.10.0, the Bitcoin Core distribution includes a consensus library.

The purpose of this library is to make the verification functionality that is
critical to Bitcoin's consensus available to other applications, e.g. to language
bindings such as [python-bitcoinlib](https://pypi.python.org/pypi/python-bitcoinlib) or
alternative node implementations.

This library is called `libbitcoinconsensus.so` (or, `.dll` for Windows).
Its interface is defined in the C header [bitcoinconsensus.h](https://github.com/bitcoin/bitcoin/blob/0.10/src/script/bitcoinconsensus.h).

In its initial version the API includes two functions:

- `bitcoinconsensus_verify_script` verifies a script. It returns whether the indicated input of the provided serialized transaction 
correctly spends the passed scriptPubKey under additional constraints indicated by flags
- `bitcoinconsensus_version` returns the API version, currently at an experimental `0`

The functionality is planned to be extended to e.g. UTXO management in upcoming releases, but the interface
for existing methods should remain stable.

Standard script rules relaxed for P2SH addresses
------------------------------------------------

The IsStandard() rules have been almost completely removed for P2SH
redemption scripts, allowing applications to make use of any valid
script type, such as "n-of-m OR y", hash-locked oracle addresses, etc.
While the Bitcoin protocol has always supported these types of script,
actually using them on mainnet has been previously inconvenient as
standard Bitcoin Core nodes wouldn't relay them to miners, nor would
most miners include them in blocks they mined.

bitcoin-tx
----------

It has been observed that many of the RPC functions offered by bitcoind are
"pure functions", and operate independently of the bitcoind wallet. This
included many of the RPC "raw transaction" API functions, such as
createrawtransaction.

bitcoin-tx is a newly introduced command line utility designed to enable easy
manipulation of bitcoin transactions. A summary of its operation may be
obtained via "bitcoin-tx --help" Transactions may be created or signed in a
manner similar to the RPC raw tx API. Transactions may be updated, deleting
inputs or outputs, or appending new inputs and outputs. Custom scripts may be
easily composed using a simple text notation, borrowed from the bitcoin test
suite.

This tool may be used for experimenting with new transaction types, signing
multi-party transactions, and many other uses. Long term, the goal is to
deprecate and remove "pure function" RPC API calls, as those do not require a
server round-trip to execute.

Other utilities "bitcoin-key" and "bitcoin-script" have been proposed, making
key and script operations easily accessible via command line.

Mining and relay policy enhancements
------------------------------------

Bitcoin Core's block templates are now for version 3 blocks only, and any mining
software relying on its `getblocktemplate` must be updated in parallel to use
libblkmaker either version 0.4.2 or any version from 0.5.1 onward.
If you are solo mining, this will affect you the moment you upgrade Bitcoin
Core, which must be done prior to BIP66 achieving its 951/1001 status.
If you are mining with the stratum mining protocol: this does not affect you.
If you are mining with the getblocktemplate protocol to a pool: this will affect
you at the pool operator's discretion, which must be no later than BIP66
achieving its 951/1001 status.

The `prioritisetransaction` RPC method has been added to enable miners to
manipulate the priority of transactions on an individual basis.

Bitcoin Core now supports BIP 22 long polling, so mining software can be
notified immediately of new templates rather than having to poll periodically.

Support for BIP 23 block proposals is now available in Bitcoin Core's
`getblocktemplate` method. This enables miners to check the basic validity of
their next block before expending work on it, reducing risks of accidental
hardforks or mining invalid blocks.

Two new options to control mining policy:
- `-datacarrier=0/1` : Relay and mine "data carrier" (OP_RETURN) transactions
if this is 1.
- `-datacarriersize=n` : Maximum size, in bytes, we consider acceptable for
"data carrier" outputs.

The relay policy has changed to more properly implement the desired behavior of not 
relaying free (or very low fee) transactions unless they have a priority above the 
AllowFreeThreshold(), in which case they are relayed subject to the rate limiter.

BIP 66: strict DER encoding for signatures
------------------------------------------

Bitcoin Core 0.10 implements BIP 66, which introduces block version 3, and a new
consensus rule, which prohibits non-DER signatures. Such transactions have been
non-standard since Bitcoin v0.8.0 (released in February 2013), but were
technically still permitted inside blocks.

This change breaks the dependency on OpenSSL's signature parsing, and is
required if implementations would want to remove all of OpenSSL from the
consensus code.

The same miner-voting mechanism as in BIP 34 is used: when 751 out of a
sequence of 1001 blocks have version number 3 or higher, the new consensus
rule becomes active for those blocks. When 951 out of a sequence of 1001
blocks have version number 3 or higher, it becomes mandatory for all blocks.

Backward compatibility with current mining software is NOT provided, thus miners
should read the first paragraph of "Mining and relay policy enhancements" above.

0.10.0 Change log
=================

Detailed release notes follow. This overview includes changes that affect external
behavior, not code moves, refactors or string updates.

RPC:
- `f923c07` Support IPv6 lookup in bitcoin-cli even when IPv6 only bound on localhost
- `b641c9c` Fix addnode "onetry": Connect with OpenNetworkConnection
- `171ca77` estimatefee / estimatepriority RPC methods
- `b750cf1` Remove cli functionality from bitcoind
- `f6984e8` Add "chain" to getmininginfo, improve help in getblockchaininfo
- `99ddc6c` Add nLocalServices info to RPC getinfo
- `cf0c47b` Remove getwork() RPC call
- `2a72d45` prioritisetransaction <txid> <priority delta> <priority tx fee>
- `e44fea5` Add an option `-datacarrier` to allow users to disable relaying/mining data carrier transactions
- `2ec5a3d` Prevent easy RPC memory exhaustion attack
- `d4640d7` Added argument to getbalance to include watchonly addresses and fixed errors in balance calculation
- `83f3543` Added argument to listaccounts to include watchonly addresses
- `952877e` Showing 'involvesWatchonly' property for transactions returned by 'listtransactions' and 'listsinceblock'. It is only appended when the transaction involves a watchonly address
- `d7d5d23` Added argument to listtransactions and listsinceblock to include watchonly addresses
- `f87ba3d` added includeWatchonly argument to 'gettransaction' because it affects balance calculation
- `0fa2f88` added includedWatchonly argument to listreceivedbyaddress/...account
- `6c37f7f` `getrawchangeaddress`: fail when keypool exhausted and wallet locked
- `ff6a7af` getblocktemplate: longpolling support
- `c4a321f` Add peerid to getpeerinfo to allow correlation with the logs
- `1b4568c` Add vout to ListTransactions output
- `b33bd7a` Implement "getchaintips" RPC command to monitor blockchain forks
- `733177e` Remove size limit in RPC client, keep it in server
- `6b5b7cb` Categorize rpc help overview
- `6f2c26a` Closely track mempool byte total. Add "getmempoolinfo" RPC
- `aa82795` Add detailed network info to getnetworkinfo RPC
- `01094bd` Don't reveal whether password is <20 or >20 characters in RPC
- `57153d4` rpc: Compute number of confirmations of a block from block height
- `ff36cbe` getnetworkinfo: export local node's client sub-version string
- `d14d7de` SanitizeString: allow '(' and ')'
- `31d6390` Fixed setaccount accepting foreign address
- `b5ec5fe` update getnetworkinfo help with subversion
- `ad6e601` RPC additions after headers-first
- `33dfbf5` rpc: Fix leveldb iterat