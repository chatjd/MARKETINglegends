# Wallet Backup Instructions

## Overview

Backing up your Vidulum private keys is the best way to be proactive about preventing loss of access to your VDL.

Problems resulting from bugs in the code, user error, device failure, etc. may lead to losing access to your wallet (and as a result, the private keys of addresses which are required to spend from them).

No matter what the cause of a corrupted or lost wallet could be, we highly recommend all users backup on a regular basis. Anytime a new address in the wallet is generated, we recommending making a new backup so all private keys for addresses in your wallet are safe.

Note that a backup is a duplicate of data needed to spend VDL so where you keep your backup(s) is another important consideration. You should not store backups where they would be equally or increasingly susceptible to loss or theft. 

## Instructions for backing up your wallet and/or private keys

These instructions are specific for the officially supported Vidulum Linux client. For backing up with third-party wallets, please consult with user guides or support channels provided for those services.

There are multiple ways to make sure you have at least one other copy of the private keys needed to spend your VDL and view your shielded VDL.

For all methods, you will need to include an export directory setting in your config file (`vidulum.conf` located in the data directory which is `~/.vidulum/` unless it's been overridden with `datadir=` setting):

`exportdir=/path/to/chosen/export