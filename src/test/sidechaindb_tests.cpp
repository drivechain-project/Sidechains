// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

BOOST_FIXTURE_TEST_SUITE(sidechaindb_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(sidechaindb_isolated)
{
    // Test SidechainDB without blocks
    uint256 hashWTTest = GetRandHash();
    uint256 hashWTHivemind = GetRandHash();
    uint256 hashWTWimble = GetRandHash();

    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];
    const Sidechain& hivemind = ValidSidechains[SIDECHAIN_HIVEMIND];
    const Sidechain& wimble = ValidSidechains[SIDECHAIN_WIMBLE];

    // SIDECHAIN_TEST
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = hashWTTest;
    // Start at +1 because we decrement in the loop
    wtTest.nBlocksLeft = test.GetTau() + 1;
    wtTest.nSidechain = SIDECHAIN_TEST;
    for (int i = 0; i <= test.nMinWorkScore; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wtTest.nWorkScore = i;
        wtTest.nBlocksLeft--;
        vWT.push_back(wtTest);
        scdb.UpdateSCDBIndex(vWT);
    }

    // SIDECHAIN_HIVEMIND
    SidechainWTPrimeState wtHivemind;
    wtHivemind.hashWTPrime = hashWTHivemind;
    // Start at +1 because we decrement in the loop
    wtHivemind.nBlocksLeft = hivemind.GetTau() + 1;
    wtHivemind.nSidechain = SIDECHAIN_HIVEMIND;
    for (int i = 0; i <= (hivemind.nMinWorkScore / 2); i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wtHivemind.nWorkScore = i;
        wtHivemind.nBlocksLeft--;
        vWT.push_back(wtHivemind);
        scdb.UpdateSCDBIndex(vWT);
    }

    // SIDECHAIN_WIMBLE
    SidechainWTPrimeState wtWimble;
    wtWimble.hashWTPrime = hashWTWimble;
    // Start at +1 because we decrement in the loop
    wtWimble.nBlocksLeft = wimble.GetTau() + 1;
    wtWimble.nSidechain = SIDECHAIN_WIMBLE;
    wtWimble.nWorkScore = 0;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtWimble);
    scdb.UpdateSCDBIndex(vWT);

    // WT^ 0 should pass with valid workscore (100/100)
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest));
    // WT^ 1 should fail with unsatisfied workscore (50/100)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_HIVEMIND, hashWTHivemind));
    // WT^ 2 should fail with unsatisfied workscore (0/100)
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_WIMBLE, hashWTWimble));

    // Reset SCDB after testing
    scdb.Reset();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MultipleTauPeriods)
{
    // Test SCDB with multiple tau periods,
    // approve multiple WT^s on the same sidechain.
    const Sidechain& test = ValidSidechains[SIDECHAIN_TEST];

    // WT^ hash for first period
    uint256 hashWTTest1 = GetRandHash();

    // Verify first transaction, check work score
    SidechainWTPrimeState wt1;
    wt1.hashWTPrime = hashWTTest1;
    // Start at +1 because we decrement in the loop
    wt1.nBlocksLeft = test.GetTau() + 1;
    wt1.nSidechain = SIDECHAIN_TEST;
    for (int i = 0; i <= test.nMinWorkScore; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt1.nWorkScore = i;
        wt1.nBlocksLeft--;
        vWT.push_back(wt1);
        scdb.UpdateSCDBIndex(vWT);
    }
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
    std::vector<SidechainWTPrimeState> vWT;
    SidechainWTPrimeState wt2;
    wt2.hashWTPrime = hashWTTest2;
    wt2.nBlocksLeft = test.GetTau();
    wt2.nSidechain = SIDECHAIN_TEST;
    wt2.nWorkScore = 0;
    vWT.push_back(wt2);
    scdb.UpdateSCDBIndex(vWT);
    BOOST_CHECK(!scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest2));

    // Verify that SCDB has updated to correct WT^
    const std::vector<SidechainWTPrimeState> vState = scdb.GetState(SIDECHAIN_TEST);
    BOOST_CHECK(vState.size() == 1 && vState[0].hashWTPrime == hashWTTest2);


    // Give second transaction sufficient workscore and check work score
    for (int i = 1; i <= test.nMinWorkScore; i++) {
        std::vector<SidechainWTPrimeState> vWT;
        wt2.nWorkScore = i;
        wt2.nBlocksLeft--;
        vWT.push_back(wt2);
        scdb.UpdateSCDBIndex(vWT);
    }
    BOOST_CHECK(scdb.CheckWorkScore(SIDECHAIN_TEST, hashWTTest2));

    // Reset SCDB after testing
    scdb.Reset();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_single)
{
    // Merkle tree based SCDB update test with only
    // SCDB data (no LD) in the tree, and a single
    // WT^ to be updated.

    // Create SCDB with initial WT^
    std::vector<SidechainWTPrimeState> vWT;

    SidechainWTPrimeState wt;
    wt.hashWTPrime = GetRandHash();
    wt.nBlocksLeft = ValidSidechains[SIDECHAIN_TEST].GetTau();
    wt.nWorkScore = 0;
    wt.nSidechain = SIDECHAIN_TEST;

    vWT.push_back(wt);
    scdb.UpdateSCDBIndex(vWT);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    vWT.clear();
    wt.nWorkScore++;
    wt.nBlocksLeft--;
    vWT.push_back(wt);
    scdbCopy.UpdateSCDBIndex(vWT);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msg;
    msg.nSidechain = SIDECHAIN_TEST;
    msg.hashWTPrime = wt.hashWTPrime;
    msg.nWorkScore = 1;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 1;
    updatePackage.vUpdate.push_back(msg);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(scdb.UpdateSCDBMatchMT(1, scdbCopy.GetHash()));

    // Reset SCDB after testing
    scdb.Reset();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleSC)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // that each have one WT^ to update. Only one WT^ out of the
    // three will be updated. This test ensures that nBlocksLeft is
    // properly decremented even when a WT^'s score is unchanged.

    // Add initial WT^s to SCDB
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = ValidSidechains[SIDECHAIN_TEST].GetTau();
    wtTest.nSidechain = SIDECHAIN_TEST;
    wtTest.nWorkScore = 0;

    SidechainWTPrimeState wtHivemind;
    wtHivemind.hashWTPrime = GetRandHash();
    wtHivemind.nBlocksLeft = ValidSidechains[SIDECHAIN_HIVEMIND].GetTau();
    wtHivemind.nSidechain = SIDECHAIN_HIVEMIND;
    wtHivemind.nWorkScore = 0;

    SidechainWTPrimeState wtWimble;
    wtWimble.hashWTPrime = GetRandHash();
    wtWimble.nBlocksLeft = ValidSidechains[SIDECHAIN_WIMBLE].GetTau();
    wtWimble.nSidechain = SIDECHAIN_WIMBLE;
    wtWimble.nWorkScore = 0;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);
    vWT.push_back(wtHivemind);
    vWT.push_back(wtWimble);

    scdb.UpdateSCDBIndex(vWT);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    wtTest.nBlocksLeft--;
    wtTest.nWorkScore++;
    wtHivemind.nBlocksLeft--;
    wtWimble.nBlocksLeft--;
    vWT.clear();
    vWT.push_back(wtTest);
    vWT.push_back(wtHivemind);
    vWT.push_back(wtWimble);

    scdbCopy.UpdateSCDBIndex(vWT);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = SIDECHAIN_TEST;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 1;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 1;
    updatePackage.vUpdate.push_back(msgTest);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(scdb.UpdateSCDBMatchMT(1, scdbCopy.GetHash()));

    // Reset SCDB after testing
    scdb.Reset();
}

BOOST_AUTO_TEST_CASE(sidechaindb_MT_multipleWT)
{
    // Merkle tree based SCDB update test with multiple sidechains
    // and multiple WT^(s) being updated. This tests that MT based
    // SCDB update will work if work scores are updated for more
    // than one sidechain per block.

    // Add initial WT^s to SCDB
    SidechainWTPrimeState wtTest;
    wtTest.hashWTPrime = GetRandHash();
    wtTest.nBlocksLeft = ValidSidechains[SIDECHAIN_TEST].GetTau();
    wtTest.nSidechain = SIDECHAIN_TEST;
    wtTest.nWorkScore = 0;

    SidechainWTPrimeState wtHivemind;
    wtHivemind.hashWTPrime = GetRandHash();
    wtHivemind.nBlocksLeft = ValidSidechains[SIDECHAIN_HIVEMIND].GetTau();
    wtHivemind.nSidechain = SIDECHAIN_HIVEMIND;
    wtHivemind.nWorkScore = 0;

    SidechainWTPrimeState wtWimble;
    wtWimble.hashWTPrime = GetRandHash();
    wtWimble.nBlocksLeft = ValidSidechains[SIDECHAIN_WIMBLE].GetTau();
    wtWimble.nSidechain = SIDECHAIN_WIMBLE;
    wtWimble.nWorkScore = 0;

    std::vector<SidechainWTPrimeState> vWT;
    vWT.push_back(wtTest);
    vWT.push_back(wtHivemind);
    vWT.push_back(wtWimble);
    scdb.UpdateSCDBIndex(vWT);

    // Create a copy of the SCDB to manipulate
    SidechainDB scdbCopy = scdb;

    // Update the SCDB copy to get a new MT hash
    wtTest.nWorkScore++;
    wtTest.nBlocksLeft--;
    wtHivemind.nBlocksLeft--;
    wtWimble.nWorkScore++;
    wtWimble.nBlocksLeft--;

    vWT.clear();
    vWT.push_back(wtTest);
    vWT.push_back(wtHivemind);
    vWT.push_back(wtWimble);

    scdbCopy.UpdateSCDBIndex(vWT);

    // Simulate receiving Sidechain WT^ update message
    SidechainUpdateMSG msgTest;
    msgTest.nSidechain = SIDECHAIN_TEST;
    msgTest.hashWTPrime = wtTest.hashWTPrime;
    msgTest.nWorkScore = 1;

    SidechainUpdateMSG msgWimble;
    msgWimble.nSidechain = SIDECHAIN_WIMBLE;
    msgWimble.hashWTPrime = wtWimble.hashWTPrime;
    msgWimble.nWorkScore = 1;

    SidechainUpdatePackage updatePackage;
    updatePackage.nHeight = 1;
    updatePackage.vUpdate.push_back(msgTest);
    updatePackage.vUpdate.push_back(msgWimble);

    scdb.AddSidechainNetworkUpdatePackage(updatePackage);

    // Use MT hash prediction to update the original SCDB
    BOOST_CHECK(scdb.UpdateSCDBMatchMT(1, scdbCopy.GetHash()));

    // Reset SCDB after testing
    scdb.Reset();
}

BOOST_AUTO_TEST_SUITE_END()
