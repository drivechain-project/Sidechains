// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "sidechain.h"
#include "uint256.h"
#include "utilstrencodings.h"

SidechainDB::SidechainDB()
{
    size_t nSidechains = ARRAYLEN(ValidSidechains);
    SCDB.resize(nSidechains );
    ratchet.resize(nSidechains);
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
            if (!IsSidechainNumberValid(nSidechain))
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

void SidechainDB::AddSidechainNetworkUpdatePackage(const SidechainUpdatePackage& update)
{
    vSidechainUpdateCache.push_back(update);
}

bool SidechainDB::AddWTPrime(uint8_t nSidechain, const CTransaction& tx)
{
    if (vWTPrimeCache.size() >= SIDECHAIN_MAX_WT)
        return false;
    if (!IsSidechainNumberValid(nSidechain))
        return false;
    if (HaveWTPrimeCached(tx.GetHash()))
        return false;

    const Sidechain& s = ValidSidechains[nSidechain];

    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.nSidechain = nSidechain;
    wt.nBlocksLeft = s.GetTau();
    wt.nWorkScore = 0;
    wt.hashWTPrime = tx.GetHash();

    vWT.push_back(wt);

    if (UpdateSCDBIndex(vWT)) {
        vWTPrimeCache.push_back(tx);
        return true;
    }
    return false;
}

int SidechainDB::CountBlocksAtop(const CCriticalData& data) const
{
    // Translate critical data into LD
    SidechainLD ld;
    ld.hashCritical = data.hashCritical;

    // Convert bytes to script for easy parsing
    CScript bytes(data.bytes.begin(), data.bytes.end());

    // Get nSidechain
    CScript::const_iterator psidechain = bytes.begin() + 3;
    opcodetype opcode;
    std::vector<unsigned char> vchSidechain;
    if (!bytes.GetOp(psidechain, opcode, vchSidechain))
        return -1;

    ld.nSidechain = CScriptNum(vchSidechain, true).getint();

    if (!IsSidechainNumberValid(ld.nSidechain))
        return -1;

    // Get prevBlockRef
    CScript::const_iterator pprevblock = bytes.begin() + 3 + vchSidechain.size();
    std::vector<unsigned char> vchPrevBlockRef;
    if (!bytes.GetOp(pprevblock, opcode, vchPrevBlockRef))
        return -1;

    ld.nPrevBlockRef = CScriptNum(vchPrevBlockRef, true).getint();

    if (ld.nPrevBlockRef > BMM_MAX_PREVBLOCK)
        return -1;

    return CountBlocksAtop(ld);
}

int SidechainDB::CountBlocksAtop(const SidechainLD& ld) const
{
    if (!IsSidechainNumberValid(ld.nSidechain))
        return -1;

    // Nothing could have matured
    if (ratchet[ld.nSidechain].size() < BMM_REQUEST_MATURITY) {
        return -1;
    }

    // Check that LD has matured
    for (size_t i = 0; i < ratchet[ld.nSidechain].size(); i++) {
        if (ratchet[ld.nSidechain][i] == ld) {
            return ratchet[ld.nSidechain].size() - i;
        }
    }

    return -1;
}

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, const uint256& hashWTPrime) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTPrimeState> vState = GetState(nSidechain);
    for (const SidechainWTPrimeState& state : vState) {
        if (state.hashWTPrime == hashWTPrime) {
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

uint256 SidechainDB::GetHash() const
{
    // TODO add LD data to the tree
    std::vector<uint256> vLeaf;
    for (const Sidechain& s : ValidSidechains) {
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        for (const SidechainWTPrimeState& state : vState) {
            vLeaf.push_back(state.GetHash());
        }
    }
    return ComputeMerkleRoot(vLeaf);
}

uint256 SidechainDB::GetHashBlockLastSeen()
{
    return hashBlockLastSeen;
}

uint256 SidechainDB::GetHashIfUpdate(const std::vector<SidechainWTPrimeState>& vNewScores) const
{
    SidechainDB scdbCopy = (*this);
    scdbCopy.UpdateSCDBIndex(vNewScores);

    return (scdbCopy.GetHash());
}

bool SidechainDB::GetLinkingData(uint8_t nSidechain, std::vector<SidechainLD>& ld) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    if (nSidechain >= ratchet.size())
        return false;

    ld = ratchet[nSidechain];

    return true;
}

std::vector<SidechainWTPrimeState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !IsSidechainNumberValid(nSidechain))
        return std::vector<SidechainWTPrimeState>();

    std::vector<SidechainWTPrimeState> vState;
    for (const SidechainWTPrimeState& member : SCDB[nSidechain].members) {
        if (!member.IsNull())
            vState.push_back(member);
    }
    return vState;
}

std::vector<CTransaction> SidechainDB::GetWTPrimeCache() const
{
    return vWTPrimeCache;
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

bool SidechainDB::HaveLinkingData(uint8_t nSidechain, uint256 hashCritical) const
{
    if (!IsSidechainNumberValid(nSidechain))
        return false;

    for (const SidechainLD& ld : ratchet[nSidechain]) {
        if (ld.hashCritical == hashCritical)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTPrimeCached(const uint256& hashWTPrime) const
{
    for (const CTransaction& tx : vWTPrimeCache) {
        if (tx.GetHash() == hashWTPrime)
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
    ratchet.clear();
    //mapBMMLD.clear();
    //std::queue<uint256> queueEmpty;
    //std::swap(queueBMMLD, queueEmpty);

    // Clear out Deposit data
    vDepositCache.clear();

    // Clear out cached WT^(s)
    vWTPrimeCache.clear();

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
        std::vector<SidechainWTPrimeState> vState = GetState(s.nSidechain);
        for (const SidechainWTPrimeState& state : vState) {
            str += state.ToString();
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
    // TODO clear out cached WT^(s) that belong to the Sidechain
    // that was just reset.

    /*
     * Now we will look for data that is relevant to SCDB
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

        if (!scriptPubKey.IsCriticalHashCommit())
            continue;

        CScript::const_iterator phash = scriptPubKey.begin() + 5;
        opcodetype opcode;
        std::vector<unsigned char> vchHash;
        if (!scriptPubKey.GetOp(phash, opcode, vchHash))
            continue;
        if (vchHash.size() != sizeof(uint256))
            continue;

        uint256 hashCritical = uint256(vchHash);

        // Read critical data bytes if there are any
        std::vector<unsigned char> bytes;
        if (scriptPubKey.size() > 37) {
            CScript::const_iterator pbytes = scriptPubKey.begin() + 38;
            if (!scriptPubKey.GetOp(pbytes, opcode, bytes))
                continue;

            // Do the bytes indicate that this is a sidechain h*?
            if (bytes[0] != 0x00 || bytes[1] != 0xbf || bytes[2] != 0x00)
                continue;

            // Read sidechain number
            CScript::const_iterator psidechain = scriptPubKey.begin() + 42;
            std::vector<unsigned char> vchSidechain;
            if (!scriptPubKey.GetOp(psidechain, opcode, vchSidechain))
                continue;

            uint8_t nSidechain = CScriptNum(vchSidechain, true).getint();

            if (!IsSidechainNumberValid(nSidechain))
                continue;

            // Read prev block ref
            CScript::const_iterator pprevblockref = psidechain + vchSidechain.size();
            std::vector<unsigned char> vchPrevBlock;
            if (!scriptPubKey.GetOp(pprevblockref, opcode, vchPrevBlock))
                continue;

            CScriptNum nPrevBlockRef(vchPrevBlock, true);
            if (nPrevBlockRef.getint() > BMM_MAX_PREVBLOCK)
                continue;
            if (nPrevBlockRef.getint() > ratchet[nSidechain].size())
                continue;

            SidechainLD ld;
            ld.nSidechain = nSidechain;
            ld.hashCritical = hashCritical;
            ld.nPrevBlockRef = nPrevBlockRef.getint();

            ratchet[nSidechain].push_back(ld);

            // Maintain ratchet size limit
            if (!(ratchet[nSidechain].size() < BMM_MAX_LD)) {
                // TODO change to vector of queue for pop()
                ratchet.erase(ratchet.begin());
            }
        }
    }

    // Scan for new WT^(s) and start tracking them
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsWTPrimeHashCommit()) {
            // Get WT^ hash from script
            CScript::const_iterator phash = scriptPubKey.begin() + 7;
            opcodetype opcode;
            std::vector<unsigned char> vchHash;
            if (!scriptPubKey.GetOp(phash, opcode, vchHash))
                continue;
            if (vchHash.size() != 32)
                continue;

            uint256 hashWT = uint256(vchHash);

            // Check sidechain number
            CScript::const_iterator pnsidechain = scriptPubKey.begin() + 39;
            std::vector<unsigned char> vchNS;
            if (!scriptPubKey.GetOp(pnsidechain, opcode, vchNS))
            if (vchNS.size() < 1 || vchNS.size() > 4)
                continue;

            CScriptNum nSidechain(vchNS, true);
            if (!IsSidechainNumberValid(nSidechain.getint()))
                continue;

            // Create WT object
            std::vector<SidechainWTPrimeState> vWT;

            SidechainWTPrimeState wt;
            wt.nSidechain = nSidechain.getint();
            wt.nBlocksLeft = ValidSidechains[nSidechain.getint()].GetTau();
            wt.nWorkScore = 0;
            wt.hashWTPrime = hashWT;

            vWT.push_back(wt);

            // Add to SCDB
            bool fUpdated = UpdateSCDBIndex(vWT);
            // TODO handle !fUpdated
        }
    }

    // Scan for updated SCDB MT hash and try to update
    // workscore of WT^(s)
    // Note: h*(s) and new WT^(s) must be added to SCDB
    // before this can be done.
    // Note: Only one MT hash commit is allowed per coinbase
    std::vector<CScript> vMTHashScript;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsSCDBHashMerkleRootCommit())
            vMTHashScript.push_back(scriptPubKey);
    }

    if (vMTHashScript.size() == 1) {
        const CScript& scriptPubKey = vMTHashScript.front();

        // Get MT hash from script
        CScript::const_iterator phash = scriptPubKey.begin() + 6;
        opcodetype opcode;
        std::vector<unsigned char> vch;
        if (scriptPubKey.GetOp(phash, opcode, vch) && vch.size() == 32) {
            // Try and sync
            uint256 hashMerkleRoot = uint256(vch);
            bool fUpdated = UpdateSCDBMatchMT(nHeight, hashMerkleRoot);
            // TODO handle !fUpdated
        }
    }

    // Update hashBLockLastSeen
    hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTPrimeState>& vNewScores)
{
    // First check that sidechain numbers are valid
    for (const SidechainWTPrimeState& s : vNewScores) {
        if (!IsSidechainNumberValid(s.nSidechain))
            return false;
    }

    // Decrement nBlocksLeft of existing WT^(s)
    for (const Sidechain& s : ValidSidechains) {
        SCDBIndex& index = SCDB[s.nSidechain];
        for (SidechainWTPrimeState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }

    // TODO
    // keep a list of the work scores that get updated, their
    // blocks remaining should have been updated as well.
    // After that, loop through again and update the
    // blocks remaining of any WT^ that wasn't in the list
    // that had their workscores updated.

    // Apply new work scores
    for (const SidechainWTPrimeState& s : vNewScores) {
        SCDBIndex& index = SCDB[s.nSidechain];
        SidechainWTPrimeState wt;
        if (index.GetMember(s.hashWTPrime, wt)) {
            // Update an existing WT^
            // Check that new work score is valid
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
            if (!s.nWorkScore == 0) // TODO == 1
                continue;
            if (s.nBlocksLeft != ValidSidechains[s.nSidechain].GetTau())
                continue;
            index.InsertMember(s);
        }
    }
    return true;
}

bool SidechainDB::UpdateSCDBMatchMT(int nHeight, const uint256& hashMerkleRoot)
{
    // First see if we are already synchronized
    if (GetHash() == hashMerkleRoot)
        return true;

    // TODO the loop below is functional. It isn't efficient.
    // Changing the container of the update cache might be a good
    // place to start.
    //
    // Try to update based on network messages
    for (const SidechainUpdatePackage& update : vSidechainUpdateCache) {
        if (update.nHeight != nHeight)
            continue;

        // Create WTPrimeState objects from the update message
        std::vector<SidechainWTPrimeState> vWT;
        for (const SidechainUpdateMSG& msg : update.vUpdate) {
            // Is sidechain number valid?
            if (!IsSidechainNumberValid(msg.nSidechain))
                 return false;

            SidechainWTPrimeState wt;
            wt.nSidechain = msg.nSidechain;
            wt.hashWTPrime = msg.hashWTPrime;
            wt.nWorkScore = msg.nWorkScore;
            wt.nBlocksLeft = ValidSidechains[wt.nSidechain].GetTau();

            // Lookup the old state (for nBlocksLeft) TODO do this better
            std::vector<SidechainWTPrimeState> vOld = GetState(wt.nSidechain);
            for (const SidechainWTPrimeState& old : vOld) {
                if (wt == old)
                    wt.nBlocksLeft = old.nBlocksLeft - 1;
            }

            vWT.push_back(wt);
        }

        // Test out updating SCDB copy with this update package
        // if it worked, apply the update
        if (GetHashIfUpdate(vWT) == hashMerkleRoot) {
            UpdateSCDBIndex(vWT);
            return (GetHash() == hashMerkleRoot);
        }
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
        for (SidechainWTPrimeState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }
    return true;
}
