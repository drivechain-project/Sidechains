// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "uint256.h"

#include <cstdint>
#include <string>
#include <vector>

struct SidechainWT {
    uint8_t nSidechain;
    CKeyID keyID;
    CMutableTransaction wt;

    std::string ToString() const;
    bool operator==(const SidechainWT& a) const;
};

class SidechainDB
{
    public:
        SidechainDB();

        /** Return const copy of current WT(s) */
        std::vector<SidechainWT> GetWTCache() const;

        /** Return the WT^ cache */
        std::vector<CTransaction> GetWTJoinCache();

        /** Update WT^ cache */
        bool AddSidechainWTJoin(const CTransaction& tx);

        /** Update wt cache */
        bool AddSidechainWT(const CTransaction& tx);

        /** Return true if cache already has wt */
        bool HaveWT(const SidechainWT& wt) const;

        /** Return true if full CTransaction with WT^ txid is cached */
        bool HaveWTJoin(const uint256& txid) const;

        /** Print the DB */
        std::string ToString() const;

    private:
        /** Sidechain wt(s) created this period */
        std::vector<SidechainWT> vWTCache;

        /** Sidechain WT^ cache */
        std::vector<CTransaction> vWTJoinCache;
};

#endif // BITCOIN_SIDECHAINDB_H

