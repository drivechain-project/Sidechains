// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include <map>
#include <queue>
#include <vector>

#include "uint256.h"

class CScript;
class CTransaction;
class CTxOut;
class uint256;

struct Sidechain;
struct SidechainDeposit;
struct SidechainLD;
struct SidechainUpdateMSG;
struct SidechainUpdatePackage;
struct SidechainWTPrimeState;
struct SCDBIndex;

class SidechainDB
{
public:
    SidechainDB();

    /** Add deposit(s) to cache */
    void AddDeposits(const std::vector<CTransaction>& vtx);

    /** Cache WT^ update TODO here for testing, move to networking */
    void AddSidechainNetworkUpdatePackage(const SidechainUpdatePackage& update);

    /** Add a new WT^ to the database */
    bool AddWTPrime(uint8_t nSidechain, const CTransaction& tx);

    /** Check that ratchet member has required BLOCKS_ATOP */
    bool CheckBlocksAtop(const SidechainLD& ld) const;

    /** Check SCDB WT^ verification status */
    bool CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime) const;

    /** Return vector of deposits this tau for nSidechain. */
    std::vector<SidechainDeposit> GetDeposits(uint8_t nSidechain) const;

    /** Return serialization hash of SCDB latest verification(s) */
    uint256 GetHash() const;

    /** Return the hash of the last block SCDB processed */
    uint256 GetHashBlockLastSeen();

    /** Return what the SCDB hash would be if the updates are applied */
    uint256 GetHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores) const;

    /**  Return BMM ratchet data for the specified sidechain, if valid */
    bool GetLinkingData(uint8_t nSidechain, std::vector<SidechainLD>& ld) const;

    /** Get status of nSidechain's WT^(s) (public for unit tests) */
    std::vector<SidechainWTPrimeState> GetState(uint8_t nSidechain) const;

    /** Return the cached WT^ transaction */
    std::vector<CTransaction> GetWTPrimeCache() const;

    /** Is there anything being tracked by the SCDB? */
    bool HasState() const;

    /** Return true if the deposit is cached */
    bool HaveDepositCached(const SidechainDeposit& deposit) const;

    /** Return true if LD is in the ratchet */
    bool HaveLinkingData(uint8_t nSidechain, uint256 hashCritical) const;

    /** Return true if the full WT^ CTransaction is cached */
    bool HaveWTPrimeCached(const uint256& hashWTPrime) const;

    /** Reset SCDB and clear out all data tracked by SidechainDB */
    void Reset();

    /** Print SCDB WT^ verification status */
    std::string ToString() const;

    /**
     * Update the DB state.
     */
    bool Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError);

    /** Update / add multiple SCDB WT^(s) to SCDB */
    bool UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores);

    /** Read the SCDB hash in a new block and try to synchronize our SCDB
     *  by testing possible work score updates until the SCDB hash of our
     *  SCDB matches that of the new block. Return false if no match found.
     */
    bool UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot);

private:
    /** Sidechain "database" tracks verification status of WT^(s) */
    std::vector<SCDBIndex> SCDB;

    /** BMM ratchet */
    std::vector<std::vector<SidechainLD>> ratchet;

    /** Cache of potential WT^ transactions */
    std::vector<CTransaction> vWTPrimeCache;

    /** Cache of deposits created during this tau */
    std::vector<SidechainDeposit> vDepositCache;

    /** Cache of WT^ update messages.
    *  TODO This is here to enable testing, remove
    *  when RPC calls are replaced with network messages.
    */
    std::vector<SidechainUpdatePackage> vSidechainUpdateCache;

    /** The most recent block that SCDB has processed */
    uint256 hashBlockLastSeen;

    /**
     * Submit default vote for all sidechain WT^(s).
     * Used when a new block does not contain a valid update.
     */
    bool ApplyDefaultUpdate();
};

#endif // BITCOIN_SIDECHAINDB_H

