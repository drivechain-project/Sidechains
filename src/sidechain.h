// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_SIDECHAIN_H
#define BITCOIN_PRIMITIVES_SIDECHAIN_H

#include "amount.h"
#include "merkleblock.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

#include <limits.h>
#include <string>
#include <vector>

/* Sidechain Identifiers */

//! Sidechain address bytes
static const std::string SIDECHAIN_ADDRESS_BYTES = "0186ff51f527ffdcf2413d50bdf8fab1feb20e5f82815dad48c73cf462b8b313";

//! Sidechain build commit hash
static const std::string SIDECHAIN_BUILD_COMMIT_HASH = "e0da09fcc3db3715e52f236ba1d849b36496e86c";

static const int MAINCHAIN_WTPRIME_VERIFICATION_PERIOD = 32;
static const int MAINCHAIN_WTPRIME_MIN_WORKSCORE = 16;

struct Sidechain {
    uint8_t nSidechain;
    CScript depositScript;

    std::string ToString() const;
    std::string GetSidechainName() const;
};

enum Sidechains {
    // This sidechain
    SIDECHAIN_TEST = 0,
};

//! WT status / zone (unspent, included in a WT^, paid out)
static const char WT_UNSPENT = 'u';
static const char WT_IN_WTPRIME = 'p';
static const char WT_SPENT = 's';

//! KeyID for testing
static const std::string testkey = "b5437dc6a4e5da5597548cf87db009237d286636";
//mx3PT9t2kzCFgAURR9HeK6B5wN8egReUxY
//cN5CqwXiaNWhNhx3oBQtA8iLjThSKxyZjfmieTsyMpG6NnHBzR7J

//! This sidechain's fee script
static const CScript SIDECHAIN_FEESCRIPT = CScript() << OP_DUP << OP_HASH160 << ToByteVector(testkey) << OP_EQUALVERIFY << OP_CHECKSIG;

//! The default payment amount to mainchain miner for critical data commitment
static const CAmount DEFAULT_CRITICAL_DATA_AMOUNT = 1 * CENT;

//! The fee for sidechain deposits on this sidechain
static const CAmount SIDECHAIN_DEPOSIT_FEE = 0.00001 * COIN;

static const char DB_SIDECHAIN_DEPOSIT_OP = 'D';
static const char DB_SIDECHAIN_DEPOSIT_KEY = 'd';

static const char DB_SIDECHAIN_WT_OP = 'W';
static const char DB_SIDECHAIN_WT_KEY = 'w';

static const char DB_SIDECHAIN_WTPRIME_OP = 'P';
static const char DB_SIDECHAIN_WTPRIME_KEY = 'p';

/**
 * Base object for sidechain related database entries
 */
struct SidechainObj {
    char sidechainop;
    uint32_t nHeight;
    uint256 txid;

    SidechainObj(void): nHeight(INT_MAX) { }
    virtual ~SidechainObj(void) { }

    uint256 GetHash(void) const;
    CScript GetScript(void) const;
    virtual std::string ToString(void) const;
};

/**
 * Sidechain individual withdrawal (WT) database object
 */
struct SidechainWT: public SidechainObj {
    uint8_t nSidechain;
    std::string strDestination;
    CAmount amount;
    char status;
    uint256 hashBlindWTX; // The hash of the WT transaction minus the WT script

    SidechainWT(void) : SidechainObj() { sidechainop = DB_SIDECHAIN_WT_OP; status = WT_UNSPENT; }
    virtual ~SidechainWT(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(nSidechain);
        READWRITE(strDestination);
        READWRITE(amount);
        READWRITE(status);
        READWRITE(hashBlindWTX);
    }

    std::string ToString(void) const;

    uint256 GetNonStatusHash() const
    {
        SidechainWT wt(*this);
        wt.status = WT_UNSPENT;
        return wt.GetHash();
    }
};

/**
 * Sidechain joined withdraw proposal (WT^) database object
 */
struct SidechainWTPrime: public SidechainObj {
    uint8_t nSidechain;
    CMutableTransaction wtPrime;
    std::vector<uint256> vWT; // The id in ldb of WT(s) that this WT^ is using

    SidechainWTPrime(void) : SidechainObj() { sidechainop = DB_SIDECHAIN_WTPRIME_OP; }
    virtual ~SidechainWTPrime(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(nSidechain);
        READWRITE(wtPrime);
        READWRITE(vWT);
    }

    uint256 GetID() const {
        SidechainWTPrime wtPrime(*this);
        wtPrime.txid.SetNull();
        return wtPrime.GetHash();
    }

    std::string ToString(void) const;
};

/**
 * Sidechain deposit database object
 */
struct SidechainDeposit : public SidechainObj {
    uint8_t nSidechain;
    CKeyID keyID;
    CAmount amtUserPayout;
    CMutableTransaction dtx;
    CMainchainMerkleBlock proof;
    uint32_t n;

    SidechainDeposit(void) : SidechainObj() { sidechainop = DB_SIDECHAIN_DEPOSIT_OP; }
    virtual ~SidechainDeposit(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(nSidechain);
        READWRITE(keyID);
        READWRITE(amtUserPayout);
        READWRITE(dtx);
        READWRITE(proof);
        READWRITE(n);
    }

    std::string ToString(void) const;

    bool operator==(const SidechainDeposit& d) const
    {
        if (sidechainop == d.sidechainop &&
                nSidechain == d.nSidechain &&
                keyID == d.keyID &&
                amtUserPayout == d.amtUserPayout &&
                dtx == d.dtx &&
                proof == d.proof &&
                n == d.n) {
            return true;
        }
        return false;
    }

    uint256 GetNonAmountHash() const
    {
        SidechainDeposit deposit(*this);
        deposit.amtUserPayout = CAmount(0);
        return deposit.GetHash();
    }
};

struct SidechainBMMProof
{
    uint256 hashBMMBlock;
    std::string txOutProof;
    std::string coinbaseHex;

    bool HasProof()
    {
        return (txOutProof.size() && coinbaseHex.size());
    }
};

/**
 * Create sidechain object
 */
SidechainObj *SidechainObjCtr(const CScript &);

#endif // BITCOIN_PRIMITIVES_SIDECHAIN_H
