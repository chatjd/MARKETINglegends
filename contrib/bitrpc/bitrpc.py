from jsonrpc import ServiceProxy
import sys
import string
import getpass

# ===== BEGIN USER SETTINGS =====
# if you do not set these you will be prompted for a password for every command
rpcuser = ""
rpcpass = ""
# ====== END USER SETTINGS ======


if rpcpass == "":
    access = ServiceProxy("http://127.0.0.1:17676")
else:
    access = ServiceProxy("http://"+rpcuser+":"+rpcpass+"@127.0.0.1:17676")
cmd = sys.argv[1].lower()

if cmd == "backupwallet":
    try:
        path = raw_input("Enter destination path/filename: ")
        print access.backupwallet(path)
    except Exception as inst:
        print inst

elif cmd == "encryptwallet":
    try:
        pwd = getpass.getpass(prompt="Enter passphrase: ")
        pwd2 = getpass.getpass(prompt="Repeat passphrase: ")
        if pwd == pwd2:
            access.encryptwallet(pwd)
            print "\n---Wallet encrypted. Server stopping, restart to run with encrypted wallet---\n"
        else:
            print "\n---Passphrases do not match---\n"
    except Exception as inst:
        print inst

elif cmd == "getaccount":
    try:
        addr = raw_input("Enter a Bitcoin address: ")
        print access.getaccount(addr)
    except Exception as inst:
        print inst

elif cmd == "getaccountaddress":
    try:
        acct = raw_input("Enter an account name: ")
        print access.getaccountaddress(acct)
    except Exception as inst:
        print inst

elif cmd == "getaddressesbyaccount":
    try:
        acct = raw_input("Enter an account name: ")
        print access.getaddressesbyaccount(acct)
    except Exception as inst:
        print inst

elif cmd == "getbalance":
    try:
        acct = raw_input("Enter an account (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getbalance(acct, mc)
        except:
            print access.getbalance()
    except Exception as inst:
        print inst

elif cmd == "getblockbycount":
    try:
        height = raw_input("Height: ")
        print access.getblockbycount(height)
    except Exception as inst:
        print inst

elif cmd == "getblockcount":
    try:
        print access.getblockcount()
    except Exception as inst:
        print inst

elif cmd == "getblocknumber":
    try:
        print access.getblocknumber()
    except Exception as inst:
        print inst

elif cmd == "getconnectioncount":
    try:
        print access.getconnectioncount()
    except Exception as inst:
        print inst

elif cmd == "getdifficulty":
    try:
        print access.getdifficulty()
    except Exception as inst:
        print inst

elif cmd == "getgenerate":
    try:
        print access.getgenerate()
    except Exception as inst:
        print inst

elif cmd == "gethashespersec":
    try:
        print access.gethashespersec()
    except Exception as inst:
        print inst

elif cmd == "getinfo":
    try:
        print access.getinfo()
    except Exception as inst:
        print inst

elif cmd == "getnewaddress":
    try:
        acct = raw_input("Enter an account name: ")
        try:
            print access.getnewaddress(acct)
        except:
            print access.getnewaddress()
    except Exception as inst:
        print inst

elif cmd == "getreceivedbyaccount":
    try:
        acct = raw_input("Enter an account (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaccount(acct, mc)
        except:
            print access.getreceivedbyaccount()
    except Exception as inst:
        print inst

elif cmd == "getreceivedbyaddress":
    try:
        addr = raw_input("Enter a Bitcoin address (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaddress(addr, mc)
        except:
            print access.getreceivedbyaddress()
    except Exception as inst:
        print inst

elif cmd == "gettransaction":
    try:
        txid = raw_input("Enter a transaction ID: ")
        print access.gettransaction(txid)
    except Exception as inst:
        print inst

elif cmd == "getwork":
    try:
        data = raw_input("Data (optional): ")
        try:
            print access.gettransaction(data)
        except:
            print access.gettransaction()
    except Exception as inst:
        print inst

elif cmd == "help":
    try:
        cmd = raw_input("Command (optional): ")
        try:
            print access.help(cmd)
        except:
            print access.help()
    except Exception as inst:
        print inst

elif cmd == "listaccounts":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.listaccounts(mc)
        except:
            print access.listaccounts()
    except Exception as inst:
        print inst

elif cmd == "listreceivedbyaccount":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
        incemp = raw_input("Include empty? (true/false, optional): ")
        try:
            print access.listreceivedbyaccount(mc, incemp)
        except:
            print access.listreceivedbyaccount()
    except Exception as inst:
        print inst

elif cmd == "listreceivedbyaddress":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
    