// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechain.h"
#include "utilstrencodings.h"

#include <sstream>

bool IsSidechainNumberValid(const uint8_t& nSidechain)
{
    if (!(nSidechain < ARRAYLEN(ValidSidechains)))
        return false;

    // Check that number corresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
    case SIDECHAIN_HIVEMIND:
    case SIDECHAIN_WIMBLE:
        return true;
    default:
        return false;
    }
}

bool CheckSidechainId(const CScriptNum& id, uint8_t& nSidechainId)
{
    if (id > CScriptNum(255) || id < CScriptNum(0)) {
        return false;
    }
    //TODO: Look at this closer, is this safe??
    nSidechainId = static_cast<uint8_t>(id.getint());
    bool result = IsSidechainNumberValid(nSidechainId);
    return result;
}

std::string Sidechain::GetSidechainName() const
{
    // Check that number corresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
        return "SIDECHAIN_TEST";
    case SIDECHAIN_HIVEMIND:
        return "SIDECHAIN_HIVEMIND";
    case SIDECHAIN_WIMBLE:
        return "SIDECHAIN_WIMBLE";
    default:
        break;
    }
    return "SIDECHAIN_UNKNOWN";
}

uint16_t Sidechain::GetTau() const
{
    return nWaitPeriod + nVerificationPeriod;
}

int Sidechain::GetLastTauHeight(int nHeight) const
{
    uint16_t nTau = GetTau();
    for (;;) {
        if (nHeight < 0)
            return -1;
        if (nHeight % nTau == 0 || nHeight == 0)
            break;
        nHeight--;
    }
    return nHeight;
}

bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.nSidechain == nSidechain &&
            a.keyID == keyID &&
            a.tx == tx);
}

bool SidechainWTJoinState::IsNull() const
{
    return (wtxid.IsNull());
}

bool SCDBIndex::IsPopulated() const
{
    // Do the least amount of work to determine whether
    // SCDBIndex is tracking anything. As the first slot
    // is populated first, we can just check if it is null.
    if (!members.front().IsNull())
        return true;

    return false;
}

bool SCDBIndex::IsFull() const
{
    for (const SidechainWTJoinState& member : members) {
        if (member.IsNull())
            return false;
    }
    return true;
}

bool SCDBIndex::InsertMember(const SidechainWTJoinState& member)
{
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i].IsNull() || members[i].wtxid == member.wtxid) {
            members[i] = member;
            return true;
        }
    }
    return false;
}

void SCDBIndex::ClearMembers()
{
    for (size_t i = 0; i < members.size(); i++)
        members[i].wtxid = uint256();
}

unsigned int SCDBIndex::CountPopulatedMembers() const
{
    unsigned int nMembers = 0;
    for (const SidechainWTJoinState& member : members) {
        if (!member.IsNull())
            nMembers++;
    }
    return nMembers;
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nWaitPeriod=" << nWaitPeriod << std::endl;
    ss << "nVerificationPeriod=" << nVerificationPeriod << std::endl;
    ss << "nMinWorkScore=" << nMinWorkScore << std::endl;
    return ss.str();
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "keyID=" << keyID.ToString() << std::endl;
    ss << "txid=" << tx.GetHash().ToString() << std::endl;
    return ss.str();
}

std::string SidechainWTJoinState::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << (unsigned int)nBlocksLeft << std::endl;
    ss << "nWorkScore=" << (unsigned int)nWorkScore << std::endl;
    ss << "wtxid=" << wtxid.ToString() << std::endl;
    return ss.str();
}
