// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "primitives/transaction.h"
#include "script/script.h"
#include "sidechain.h"
#include "uint256.h"
#include "utilstrencodings.h"

SidechainDB::SidechainDB()
{
    SCDB.resize(ARRAYLEN(ValidSidechains));
}

void SidechainDB::AddDeposits(const std::vector<CTransaction>& vtx)
{
    std::vector<SidechainDeposit> vDeposit;
    for (const CTransaction& tx : vtx) {
        // Create sidechain deposit objects from transaction outputs
        for (const CTxOut& out : tx.vout) {
            const CScript &scriptPubKey = out.scriptPubKey;

            // scriptPubKey must contain keyID
            if (scriptPubKey.size() < sizeof(uint160))
                continue;
            if (scriptPubKey.front() != OP_RETURN)
                continue;

            uint8_t nSidechain = (unsigned int)scriptPubKey[1];
            if (!SidechainNumberValid(nSidechain))
                continue;

            CScript::const_iterator pkey = scriptPubKey.begin() + 2;
            opcodetype opcode;
            std::vector<unsigned char> vch;
            if (!scriptPubKey.GetOp(pkey, opcode, vch))
                continue;
            if (vch.size() != sizeof(uint160))
                continue;

            CKeyID keyID = CKeyID(uint160(vch));
            if (keyID.IsNull())
                continue;

            SidechainDeposit deposit;
            deposit.tx = tx;
            deposit.keyID = keyID;
            deposit.nSidechain = nSidechain;

            vDeposit.push_back(deposit);
        }
    }

    // Add deposits to cache
    for (const SidechainDeposit& d : vDeposit) {
        if (!HaveDepositCached(d))
            vDepositCache.push_back(d);
    }
}

bool SidechainDB::AddWTJoin(uint8_t nSidechain, const CTransaction& tx)
{
    if (vWTJoinCache.size() >= SIDECHAIN_MAX_WT)
        return false;
    if (!SidechainNumberValid(nSidechain))
        return false;
    if (HaveWTJoinCached(tx.GetHash()))
        return false;

    const Sidechain& s = ValidSidechains[nSidechain];
    if (UpdateSCDBIndex(nSidechain, s.GetTau(), 0, tx.GetHash())) {
        vWTJoinCache.push_back(tx);
        return true;
    }
    return false;
}

bool SidechainDB::ApplyDefaultUpdate()
{
    if (!HasState())
        return true;

    // Decrement nBlocksLeft, nothing else changes
    for (const Sidechain& s : ValidSidechains) {
        SCDBIndex& index = SCDB[s.nSidechain];
        for (SidechainWTJoinState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }
    return true;
}

bool SidechainDB::CheckWorkScore(const uint8_t& nSidechain, const uint256& wtxid) const
{
    if (!SidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTJoinState> vState = GetState(nSidechain);
    for (const SidechainWTJoinState& state : vState) {
        if (state.wtxid == wtxid) {
            if (state.nWorkScore >= ValidSidechains[nSidechain].nMinWorkScore)
                return true;
            else
                return false;
        }
    }
    return false;
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(uint8_t nSidechain) const
{
    std::vector<SidechainDeposit> vSidechainDeposit;
    for (size_t i = 0; i < vDepositCache.size(); i++) {
        if (vDepositCache[i].nSidechain == nSidechain)
            vSidechainDeposit.push_back(vDepositCache[i]);
    }
    return vSidechainDeposit;
}

uint256 SidechainDB::GetHashBlockLastSeen()
{
    return hashBlockLastSeen;
}

std::multimap<uint256, int> SidechainDB::GetLinkingData() const
{
    return mapBMMLD;
}

std::vector<SidechainWTJoinState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !SidechainNumberValid(nSidechain))
        return std::vector<SidechainWTJoinState>();

    std::vector<SidechainWTJoinState> vState;
    for (const SidechainWTJoinState& member : SCDB[nSidechain].members) {
        if (!member.IsNull())
            vState.push_back(member);
    }
    return vState;
}

std::vector<CTransaction> SidechainDB::GetWTJoinCache() const
{
    return vWTJoinCache;
}

bool SidechainDB::HasState() const
{
    // Make sure that SCDB is actually initialized
    if (SCDB.size() != ARRAYLEN(ValidSidechains))
        return false;

    // Check if any SCDBIndex(s) are populated
    if (SCDB[SIDECHAIN_TEST].IsPopulated())
        return true;
    else
    if (SCDB[SIDECHAIN_HIVEMIND].IsPopulated())
        return true;
    else
    if (SCDB[SIDECHAIN_WIMBLE].IsPopulated())
        return true;

    return false;
}

bool SidechainDB::HaveDepositCached(const SidechainDeposit &deposit) const
{
    for (const SidechainDeposit& d : vDepositCache) {
        if (d == deposit)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTJoinCached(const uint256& wtxid) const
{
    for (const CTransaction& tx : vWTJoinCache) {
        if (tx.GetHash() == wtxid)
            return true;
    }
    return false;
}

void SidechainDB::Reset()
{
    // Clear out SCDB
    for (const Sidechain& s : ValidSidechains)
        SCDB[s.nSidechain].ClearMembers();

    // Clear out LD
    mapBMMLD.clear();
    std::queue<uint256> queueEmpty;
    std::swap(queueBMMLD, queueEmpty);

    // Clear out Deposit data
    vDepositCache.clear();

    // Clear out cached WT^(s)
    vWTJoinCache.clear();

    // Reset hashBlockLastSeen
    hashBlockLastSeen.SetNull();
}

std::string SidechainDB::ToString() const
{
    std::string str;
    str += "SidechainDB:\n";
    for (const Sidechain& s : ValidSidechains) {
        // Print sidechain name
        str += "Sidechain: " + s.GetSidechainName() + "\n";
        // Print sidechain WT^ workscore(s)
        std::vector<SidechainWTJoinState> vState = GetState(s.nSidechain);
        for (const SidechainWTJoinState& state : vState) {
            str += "WT^: " + state.wtxid.ToString() + "\n";
            str += "workscore: " + std::to_string(state.nWorkScore) + "\n";
        }
        str += "\n";
    }
    return str;
}

bool SidechainDB::Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError)
{
    if (hashBlock.IsNull())
        return false;
    if (!vout.size())
        return false;

    // TODO skip if nHeight < drivechains activation block height

    // If a sidechain's tau period ended, reset WT^ verification status
    for (const Sidechain& s : ValidSidechains) {
        if (nHeight > 0 && (nHeight % s.GetTau()) == 0)
            SCDB[s.nSidechain].ClearMembers();
    }
    // TODO also clear out cached WT^(s) to save memory

    /*
     * Now we will look for data that is relevent to SCDB
     * in this block's coinbase.
     *
     * Scan for h* linking data and add it to the BMMLD
     * ratchet system.
     *
     * Scan for new WT^(s) and start tracking them.
     *
     * Scan for updated SCDB MT hash, and perform MT hash
     * based SCDB update.
     *
     * Update hashBlockLastSeen to reflect that we have
     * scanned this latest block.
     */

    // Scan for h*(s)
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        // Must at least contain the h*
        if (scriptPubKey.size() < sizeof(uint256))
            continue;
        if (!scriptPubKey.IsUnspendable())
            continue;

        CScript::const_iterator pbn = scriptPubKey.begin() + 1;
        opcodetype opcode;
        std::vector<unsigned char> vchBN;
        if (!scriptPubKey.GetOp(pbn, opcode, vchBN))
            continue;
        if (vchBN.size() < 1 || vchBN.size() > 4)
            continue;

        CScriptNum nBlock(vchBN, true);

        CScript::const_iterator phash = scriptPubKey.begin() + vchBN.size() + 2;
        std::vector<unsigned char> vch;
        if (!scriptPubKey.GetOp(phash, opcode, vch))
            continue;
        if (vch.size() != sizeof(uint256))
            continue;

        uint256 hashCritical = uint256(vch);

        // Check block number
        bool fValid = false;
        if (queueBMMLD.size()) {
            // Compare block number with most recent h* block number
            uint256 hashMostRecent = queueBMMLD.back();
            std::multimap<uint256, int>::const_iterator it = mapBMMLD.find(hashMostRecent);
            if (it == mapBMMLD.end())
                return false;

            int nHeightMostRecent = it->second;

            if ((nBlock.getint() - nHeightMostRecent) <= 1)
                fValid = true;
        } else {
            // No previous h* to compare with
            fValid = true;
        }
        if (!fValid) {
            strError = "SidechainDB::Update: h* invalid";
            continue;
        }

        // Update BMM linking data
        // Add new linking data
        mapBMMLD.emplace(hashCritical, nBlock.getint());
        queueBMMLD.push(hashCritical);

        // Remove old linking data if we need to
        if (mapBMMLD.size() > SIDECHAIN_MAX_LD) {
            uint256 hashRemove = queueBMMLD.front();
            std::multimap<uint256, int>::const_iterator it = mapBMMLD.lower_bound(hashRemove);
            if (it->first == hashRemove) {
                mapBMMLD.erase(it);
                queueBMMLD.pop();
            }
        }
    }

    // Scan for new WT^(s) and start tracking them
    // TODO
    // SidechainDB::AddWTJoin

    // Scan for updated SCDB MT hash and try to update
    // workscore of WT^(s)
    // Note: h*(s) and new WT^(s) must be added to SCDB
    // before this can be done.

    // Update hashBLockLastSeen
    hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTJoinState>& vNewScores)
{
    // First check that sidechain numbers are valid
    for (const SidechainWTJoinState& s : vNewScores) {
        if (!SidechainNumberValid(s.nSidechain))
            return false;
    }

    // Now decrement nBlocksLeft of all WT^(s)
    for (const Sidechain& s : ValidSidechains) {
        SCDBIndex& index = SCDB[s.nSidechain];
        for (SidechainWTJoinState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }

    // Finally apply new scores to WT^(s)
    for (const SidechainWTJoinState& s : vNewScores) {
        SCDBIndex& index = SCDB[s.nSidechain];
        SidechainWTJoinState wt;
        if (index.GetMember(s.wtxid, wt)) {
            // Update an existing WT^
            // TODO use abs instead (caused build issues).
            if ((wt.nWorkScore == s.nWorkScore) ||
                    (s.nWorkScore == (wt.nWorkScore + 1)) ||
                    (s.nWorkScore == (wt.nWorkScore - 1)))
            {
                index.InsertMember(s);
            }
        }
        else
        if (!index.IsFull()) {
            // Add a new WT^
            if (!s.nWorkScore == 0)
                continue;
            if (s.nBlocksLeft != ValidSidechains[s.nSidechain].GetTau())
                continue;
            index.InsertMember(s);
        }
    }
    return true;
}

bool SidechainDB::UpdateSCDBIndex(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid)
{
    if (!SidechainNumberValid(nSidechain))
        return false;

    SidechainWTJoinState wt;
    wt.nBlocksLeft = nBlocks;
    wt.nSidechain = nSidechain;
    wt.nWorkScore = nScore;
    wt.wtxid = wtxid;

    std::vector<SidechainWTJoinState> vNewScores;
    vNewScores.push_back(wt);

    // Insert member
    return (UpdateSCDBIndex(vNewScores));
}
