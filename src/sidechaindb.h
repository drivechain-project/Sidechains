// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include "uint256.h"

#include <map>
#include <queue>
#include <vector>

class CScript;
class CTransaction;
class CTxOut;
class uint256;

struct Sidechain;
struct SidechainDeposit;
struct SidechainWTJoinState;
struct SCDBIndex;

class SidechainDB
{
public:
    SidechainDB();

    /** Add deposit(s) to cache */
    void AddDeposits(const std::vector<CTransaction>& vtx);

    /** Add a new WT^ to the database */
    bool AddWTJoin(uint8_t nSidechain, const CTransaction& tx);

    /** Check SCDB WT^ verification status */
    bool CheckWorkScore(const uint8_t& nSidechain, const uint256& wtxid) const;

    /** Return vector of deposits this tau for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(uint8_t nSidechain) const;

    /** Return the hash of the last block SCDB processed */
    uint256 GetHashBlockLastSeen();

    /**
     * Return from the BMM ratchet the data which is required to
     * validate an OP_BRIBE script.
     */
    std::multimap<uint256, int> GetLinkingData() const;

    /** Get status of nSidechain's WT^(s) (public for unit tests) */
    std::vector<SidechainWTJoinState> GetState(uint8_t nSidechain) const;

    /** Return the cached WT^ transaction */
    std::vector<CTransaction> GetWTJoinCache() const;

    /** Is there anything being tracked by the SCDB? */
    bool HasState() const;

    /** Return true if the deposit is cached */
    bool HaveDepositCached(const SidechainDeposit& deposit) const;

    /** Return true if the full WT^ CTransaction is cached */
    bool HaveWTJoinCached(const uint256& wtxid) const;

    /** Reset SCDB and clear out all data tracked by SidechainDB */
    void Reset();

    /** Print SCDB WT^ verification status */
    std::string ToString() const;

    /**
     * Update the DB state. This function is the only function that
     * updates the SCDB state during normal operation. The update
     * overload exists to facilitate testing.
     */
    bool Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError);

    /** Update / add multiple SCDB WT^(s) to SCDB */
    bool UpdateSCDBIndex(const std::vector<SidechainWTJoinState>& vNewScores);

    /** Update / add single WT^(s) to SCDB */
    bool UpdateSCDBIndex(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid);

private:
    /** Sidechain "database" tracks verification status of WT^(s) */
    std::vector<SCDBIndex> SCDB;

    /** BMM ratchet */
    std::multimap<uint256, int> mapBMMLD;
    std::queue<uint256> queueBMMLD;

    /** Cache of potential WT^ transactions */
    std::vector<CTransaction> vWTJoinCache;

    /** Cache of deposits created during this tau */
    std::vector<SidechainDeposit> vDepositCache;

    /** The most recent block that SCDB has processed */
    uint256 hashBlockLastSeen;

    /**
     * Submit default vote for all sidechain WT^(s).
     * Used when a new block does not contain a valid update.
     */
    bool ApplyDefaultUpdate();
};

#endif // BITCOIN_SIDECHAINDB_H

