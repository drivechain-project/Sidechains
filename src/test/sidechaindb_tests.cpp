// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#include <stdlib.h>

#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/sigcache.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

//! KeyID for testing
// mx3PT9t2kzCFgAURR9HeK6B5wN8egReUxY
// cN5CqwXiaNWhNhx3oBQtA8iLjThSKxyZjfmieTsyMpG6NnHBzR7J
static const std::string testKey = "b5437dc6a4e5da5597548cf87db009237d286636";

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(sidechaindb_isolated)
{
    // Test SidechainDB without blocks
    scdb.Reset();

    uint256 hashWTTest = GetRandHash();
    uint256 hashWTHivemind = GetRandHash();
    uint256 hashWTWimble = GetRandHash();

    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];
    const Sidechain& hivemind = ValidSidechains[SIDECHAIN_TEST];
    const Sidechain& wimble = ValidSidechains[SIDECHAIN_TEST];

    // SIDECHAIN_TEST
    for (int i = 0; i <= test.nMinWorkScore; i++) {
        scdb.UpdateSCDBIndex(SIDECHAIN_TEST, test.GetTau() - i, i, hashWTTest);
    }

    // SIDECHAIN_HIVEMIND
    for (int i = 0; i <= (hivemind.nMinWorkScore / 2); i++) {
        scdb.UpdateSCDBIndex(SIDECHAIN_HIVEMIND, hivemind.GetTau() - i, i, hashWTHivemind);
    }

    // SIDECHAIN_WIMBLE
    scdb.UpdateSCDBIndex(SIDECHAIN_WIMBLE, wimble.GetTau(), 0, hashWTWimble);


    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest));
    // WT^ 1 should fail with unsatisfied workscore (50/100)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_HIVEMIND, hashWTHivemind));
    // WT^ 2 should fail with unsatisfied workscore (0/100)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_WIMBLE, hashWTWimble));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MultipleTauPeriods)
{
    // Test SCDB with multiple tau periods,
    // approve multiple WT^s on the same sidechain.
    scdb.Reset();
    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];

    // WT^ hash for first period
    uint256 hashWTTest1 = GetRandHash();

    // Verify first transaction, check work score
    for (int i = 0; i <= test.nMinWorkScore; i++)
        scdb.UpdateSCDBIndex(SIDECHAIN_TEST, test.GetTau() - i, i, hashWTTest1);
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest1));

    // Create dummy coinbase tx
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, CScript() << OP_RETURN));

    uint256 hashBlock = GetRandHash();

    // Update SCDB (will clear out old data from first period)
    std::string strError = "";
    scdb.Update(test.GetTau(), hashBlock, mtx.vout, strError);

    // WT^ hash for second period
    uint256 hashWTTest2 = GetRandHash();

    // Add new WT^
    scdb.UpdateSCDBIndex(SIDECHAIN_TEST, test.GetTau(), 0, hashWTTest2);
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest2));

    // Verify that SCDB has updated to correct WT^
    const std::vector<SidechainWTJoinState> vState = scdb.GetState(SIDECHAIN_TEST);
    BOOST_CHECK(vState.size() == 1 && vState[0].wtxid == hashWTTest2);

    // Give second transaction sufficient workscore and check work score
    for (int i = 1; i <= test.nMinWorkScore; i++) {
        scdb.UpdateSCDBIndex(SIDECHAIN_TEST, test.GetTau() - i, i, hashWTTest2);

    }
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest2));
}

// TODO move BMM tests into seperate file
BOOST_AUTO_TEST_CASE(bmm_checkCriticalHashValid)
{
    // Check that valid critical hash is added to ratchet
    scdb.Reset();

    // Create h* bribe script
    uint256 hashCritical = GetRandHash();
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(1) << ToByteVector(hashCritical);

    // Create dummy coinbase with h*
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was added
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical);
    BOOST_CHECK(it != mapLD.end());

}

BOOST_AUTO_TEST_CASE(bmm_checkCriticalHashInvalid)
{
    // Make sure that a invalid h* with a valid block number will
    // be rejected.
    scdb.Reset();

    // Create h* bribe script
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(21000) << 0x426974636F696E;

    // Create dummy coinbase with h*
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was rejected, linking data should be empty
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();
    BOOST_CHECK(mapLD.empty());
}

BOOST_AUTO_TEST_CASE(bmm_checkBlockNumberValid)
{
    // Make sure that a valid h* with a valid block number
    // will be accepted.
    scdb.Reset();

    // We have to add the first h* to the ratchet so that
    // there is something to compare with.

    // Create h* bribe script
    uint256 hashCritical = GetRandHash();
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(1) << ToByteVector(hashCritical);

    // Create dummy coinbase with h* in output
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that first h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Now we add a second h* with a valid block number

    // Create second h* bribe script
    uint256 hashCritical2 = GetRandHash();
    CScript scriptPubKey2;
    scriptPubKey2 << OP_RETURN << CScriptNum::serialize(2) << ToByteVector(hashCritical2);

    // Update SCDB so that second h* is processed
    mtx.vout[0] = CTxOut(50 * CENT, scriptPubKey2);
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was added
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical2);
    BOOST_CHECK(it != mapLD.end());
}

BOOST_AUTO_TEST_CASE(bmm_checkBlockNumberInvalid)
{
    // Try to add a valid h* with an invalid block number
    // and make sure it is skipped.
    scdb.Reset();

    // We have to add the first h* to the ratchet so that
    // there is something to compare with.

    // Create first h* bribe script
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN << CScriptNum::serialize(10) << ToByteVector(GetRandHash());

    // Create dummy coinbase with h* in output
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].scriptSig = CScript() << 486604799;
    mtx.vout.push_back(CTxOut(50 * CENT, scriptPubKey));

    // Update SCDB so that first h* is processed
    std::string strError = "";
    scdb.Update(0, GetRandHash(), mtx.vout, strError);

    // Now we add a second h* with an invalid block number

    // Create second h* bribe script
    uint256 hashCritical2 = GetRandHash();
    CScript scriptPubKey2;
    scriptPubKey2 << OP_RETURN << CScriptNum::serialize(100) << ToByteVector(hashCritical2);

    // Update SCDB so that second h* is processed
    CMutableTransaction mtx1;
    mtx1.nVersion = 1;
    mtx1.vin.resize(1);
    mtx1.vout.resize(1);
    mtx1.vin[0].scriptSig = CScript() << 486604899;
    mtx1.vout.push_back(CTxOut(50 * CENT, scriptPubKey2));

    scdb.Update(1, GetRandHash(), mtx1.vout, strError);

    // Get linking data
    std::multimap<uint256, int> mapLD = scdb.GetLinkingData();

    // Verify that h* was rejected
    std::multimap<uint256, int>::const_iterator it = mapLD.find(hashCritical2);
    BOOST_CHECK(it == mapLD.end());
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_single)
{
    // Merkle tree based SCDB update test with only
    // SCDB data (no LD) in the tree, and a single
    // WT^ to be updated.
    scdb.Reset();

    // Create SCDB with initial WT^
    uint256 hashWT = GetRandHash();
    scdb.UpdateSCDBIndex(SIDECHAIN_TEST, 300, 0, hashWT);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_TEST, 299, 1, hashWT);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(UpdateSCDBMatchMT(GetSCDBHash(scdbCopy)));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleSC)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // that each have one WT^ to update. Only one WT^ out of the
    // three will be updated. This test ensures that nBlocksLeft is
    // properly decremented even when a WT^'s score is unchanged.
    scdb.Reset();

    // Add initial WT^s to SCDB
    uint256 hashWT1 = GetRandHash();
    uint256 hashWT2 = GetRandHash();
    uint256 hashWT3 = GetRandHash();
    scdb.UpdateSCDBIndex(SIDECHAIN_TEST, 300, 0, hashWT1);
    scdb.UpdateSCDBIndex(SIDECHAIN_HIVEMIND, 300, 0, hashWT2);
    scdb.UpdateSCDBIndex(SIDECHAIN_WIMBLE, 300, 0, hashWT3);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_TEST, 299, 1, hashWT1);
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_HIVEMIND, 299, 0, hashWT2);
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_WIMBLE, 299, 0, hashWT3);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(UpdateSCDBMatchMT(GetSCDBHash(scdbCopy)));
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleWT)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // and multiple WT^(s) being updated. This tests that MT based
    // SCDB update will work if work scores are updated for more
    // than one sidechain per block.
    scdb.Reset();

    // Add initial WT^s to SCDB
    uint256 hashWT1 = GetRandHash();
    uint256 hashWT2 = GetRandHash();
    uint256 hashWT3 = GetRandHash();
    scdb.UpdateSCDBIndex(SIDECHAIN_TEST, 300, 0, hashWT1);
    scdb.UpdateSCDBIndex(SIDECHAIN_HIVEMIND, 300, 0, hashWT2);
    scdb.UpdateSCDBIndex(SIDECHAIN_WIMBLE, 300, 0, hashWT3);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_TEST, 299, 1, hashWT1);
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_HIVEMIND, 299, 0, hashWT2);
    scdbCopy.UpdateSCDBIndex(SIDECHAIN_WIMBLE, 299, 1, hashWT3);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(UpdateSCDBMatchMT(GetSCDBHash(scdbCopy)));
}

BOOST_AUTO_TEST_SUITE_END()
