// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_SIDECHAIN_H
#define BITCOIN_PRIMITIVES_SIDECHAIN_H

#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include <limits.h>
#include <string>
#include <vector>

using namespace std;

struct Sidechain {
    uint8_t nSidechain;
    uint16_t nWaitPeriod;
    uint16_t nVerificationPeriod;
    uint16_t nMinWorkScore;
    CScript depositScript;

    std::string ToString() const;
    std::string GetSidechainName() const;
};

enum Sidechains {
    SIDECHAIN_TEST = 0,
};

//! KeyID for testing
static const std::string testkey = "b5437dc6a4e5da5597548cf87db009237d286636";
//mx3PT9t2kzCFgAURR9HeK6B5wN8egReUxY
//cN5CqwXiaNWhNhx3oBQtA8iLjThSKxyZjfmieTsyMpG6NnHBzR7J

//! This sidechain as it has been described to the mainchain
static const Sidechain THIS_SIDECHAIN = {
    SIDECHAIN_TEST, 100, 200, 100, CScript() << THIS_SIDECHAIN.nSidechain << ToByteVector(testkey) << OP_NOP4
};

//! This sidechain's fee script
static const CScript SIDECHAIN_FEESCRIPT = CScript() << OP_DUP << OP_HASH160 << ToByteVector(testkey) << OP_EQUALVERIFY << OP_CHECKSIG;
//! Max number of WT^(s) per sidechain per period
static const int SIDECHAIN_MAX_WT = 3;
//! State script version number
static const int SIDECHAIN_STATE_VERSION = 0;

/**
 * Sidechain object for database
 */
struct SidechainObj {
    char sidechainop;
    uint32_t nHeight;
    uint256 txid;

    SidechainObj(void): nHeight(INT_MAX) { }
    virtual ~SidechainObj(void) { }

    uint256 GetHash(void) const;
    CScript GetScript(void) const;
    virtual string ToString(void) const;
};

/**
 * Sidechain withdraw proposal added to database
 */
struct SidechainWTJoin: public SidechainObj {
    uint8_t nSidechain;
    CMutableTransaction wtJoin;

    SidechainWTJoin(void) : SidechainObj() { sidechainop = 'W'; }
    virtual ~SidechainWTJoin(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(nSidechain);
        READWRITE(wtJoin);
    }

    string ToString(void) const;
};

/**
 * Sidechain deposit added to database
 */
struct SidechainDeposit : public SidechainObj {
    uint8_t nSidechain;
    CKeyID keyID;
    CMutableTransaction dtx;

    SidechainDeposit(void) : SidechainObj() { sidechainop = 'D'; }
    virtual ~SidechainDeposit(void) { }

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(sidechainop);
        READWRITE(nSidechain);
        READWRITE(dtx);
        READWRITE(keyID);
    }

    string ToString(void) const;
};

/**
 * Create sidechain object
 */
SidechainObj *SidechainObjCtr(const CScript &);

#endif // BITCOIN_PRIMITIVES_SIDECHAIN_H
