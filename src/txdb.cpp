// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txdb.h>

#include <chainparams.h>
#include <consensus/params.h>
#include <hash.h>
#include <random.h>
#include <sidechain.h>
#include <uint256.h>
#include <util.h>
#include <ui_interface.h>
#include <init.h>

#include <stdint.h>

#include <boost/thread.hpp>

static const char DB_COIN = 'C';
static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_HEAD_BLOCKS = 'H';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';

static const char DB_LAST_SIDECHAIN_DEPOSIT = 'x';
static const char DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE = 'w';

static const char DB_ASSET = 'A';
static const char DB_ASSET_LAST_ID = 'I';

namespace {

struct CoinEntry {
    COutPoint* outpoint;
    char key;
    explicit CoinEntry(const COutPoint* ptr) : outpoint(const_cast<COutPoint*>(ptr)), key(DB_COIN)  {}

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << key;
        s << outpoint->hash;
        s << VARINT(outpoint->n);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> key;
        s >> outpoint->hash;
        s >> VARINT(outpoint->n);
    }
};

}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe, true)
{
}

bool CCoinsViewDB::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    return db.Read(CoinEntry(&outpoint), coin);
}

bool CCoinsViewDB::HaveCoin(const COutPoint &outpoint) const {
    return db.Exists(CoinEntry(&outpoint));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

std::vector<uint256> CCoinsViewDB::GetHeadBlocks() const {
    std::vector<uint256> vhashHeadBlocks;
    if (!db.Read(DB_HEAD_BLOCKS, vhashHeadBlocks)) {
        return std::vector<uint256>();
    }
    return vhashHeadBlocks;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    size_t batch_size = (size_t)gArgs.GetArg("-dbbatchsize", nDefaultDbBatchSize);
    int crash_simulate = gArgs.GetArg("-dbcrashratio", 0);
    assert(!hashBlock.IsNull());

    uint256 old_tip = GetBestBlock();
    if (old_tip.IsNull()) {
        // We may be in the middle of replaying.
        std::vector<uint256> old_heads = GetHeadBlocks();
        if (old_heads.size() == 2) {
            assert(old_heads[0] == hashBlock);
            old_tip = old_heads[1];
        }
    }

    // In the first batch, mark the database as being in the middle of a
    // transition from old_tip to hashBlock.
    // A vector is used for future extensibility, as we may want to support
    // interrupting after partial writes from multiple independent reorgs.
    batch.Erase(DB_BEST_BLOCK);
    batch.Write(DB_HEAD_BLOCKS, std::vector<uint256>{hashBlock, old_tip});

    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CoinEntry entry(&it->first);
            if (it->second.coin.IsSpent())
                batch.Erase(entry);
            else
                batch.Write(entry, it->second.coin);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
        if (batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::COINDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            db.WriteBatch(batch);
            batch.Clear();
            if (crash_simulate) {
                static FastRandomContext rng;
                if (rng.randrange(crash_simulate) == 0) {
                    LogPrintf("Simulating a crash. Goodbye.\n");
                    _Exit(0);
                }
            }
        }
    }

    // In the last batch, mark the database as consistent with hashBlock again.
    batch.Erase(DB_HEAD_BLOCKS);
    batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint(BCLog::COINDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    bool ret = db.WriteBatch(batch);
    LogPrint(BCLog::COINDB, "Committed %u changed transaction outputs (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return ret;
}

size_t CCoinsViewDB::EstimateSize() const
{
    return db.EstimateSize(DB_COIN, (char)(DB_COIN+1));
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(std::make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper&>(db).NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COIN);
    // Cache key of first record
    if (i->pcursor->Valid()) {
        CoinEntry entry(&i->keyTmp.second);
        i->pcursor->GetKey(entry);
        i->keyTmp.first = entry.key;
    } else {
        i->keyTmp.first = 0; // Make sure Valid() and GetKey() return false
    }
    return i;
}

bool CCoinsViewDBCursor::GetKey(COutPoint &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COIN) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(Coin &coin) const
{
    return pcursor->GetValue(coin);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COIN;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    CoinEntry entry(&keyTmp.second);
    if (!pcursor->Valid() || !pcursor->GetKey(entry)) {
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
    } else {
        keyTmp.first = entry.key;
    }
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(std::make_pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(std::make_pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&, const uint256&)> insertBlockIndex)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
                CBlockIndex* pindexNew = insertBlockIndex(diskindex.GetBlockHash(), diskindex.hashMainBlock);
                pindexNew->pprev          = insertBlockIndex(diskindex.hashPrev, uint256());
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->hashMainBlock  = diskindex.hashMainBlock;
                pindexNew->hashWithdrawalBundle    = diskindex.hashWithdrawalBundle;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;

                pcursor->Next();
            } else {
                return error("%s: failed to read value", __func__);
            }
        } else {
            break;
        }
    }

    return true;
}

CSidechainTreeDB::CSidechainTreeDB(size_t nCacheSize, bool fMemory, bool fWipe)
    : CDBWrapper(GetDataDir() / "blocks" / "sidechain", nCacheSize, fMemory, fWipe) { }

bool CSidechainTreeDB::WriteSidechainIndex(const std::vector<std::pair<uint256, const SidechainObj *> > &list)
{
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256, const SidechainObj *> >::const_iterator it=list.begin(); it!=list.end(); it++) {
        const uint256 &objid = it->first;
        const SidechainObj *obj = it->second;
        std::pair<char, uint256> key = std::make_pair(obj->sidechainop, objid);

        if (obj->sidechainop == DB_SIDECHAIN_WITHDRAWAL_OP) {
            const SidechainWithdrawal *ptr = (const SidechainWithdrawal *) obj;
            batch.Write(key, *ptr);
        }
        else
        if (obj->sidechainop == DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP) {
            const SidechainWithdrawalBundle *ptr = (const SidechainWithdrawalBundle *) obj;
            batch.Write(key, *ptr);

            // Also index the WithdrawalBundle by the WithdrawalBundle transaction hash
            uint256 hashWithdrawalBundle = ptr->tx.GetHash();
            std::pair<char, uint256> keyTx = std::make_pair(DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP, hashWithdrawalBundle);
            batch.Write(keyTx, *ptr);

            // Update DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE
            batch.Write(DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE, hashWithdrawalBundle);

            LogPrintf("%s: Writing new WithdrawalBundle and updating DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE to: %s",
                    __func__, hashWithdrawalBundle.ToString());
        }
        else
        if (obj->sidechainop == DB_SIDECHAIN_DEPOSIT_OP) {
            const SidechainDeposit *ptr = (const SidechainDeposit *) obj;
            batch.Write(key, *ptr);

            // Also index the deposit by the non amount hash
            uint256 hashNonAmount = ptr->GetID();
            batch.Write(std::make_pair(DB_SIDECHAIN_DEPOSIT_OP, hashNonAmount), *ptr);

            // Update DB_LAST_SIDECHAIN_DEPOSIT
            batch.Write(DB_LAST_SIDECHAIN_DEPOSIT, hashNonAmount);
        }
    }

    return WriteBatch(batch, true);
}

bool CSidechainTreeDB::WriteWithdrawalUpdate(const std::vector<SidechainWithdrawal>& vWithdrawal)
{
    CDBBatch batch(*this);

    for (const SidechainWithdrawal& wt : vWithdrawal)
    {
        std::pair<char, uint256> key = std::make_pair(wt.sidechainop, wt.GetID());
        batch.Write(key, wt);
    }

    return WriteBatch(batch, true);
}

bool CSidechainTreeDB::WriteWithdrawalBundleUpdate(const SidechainWithdrawalBundle& withdrawalBundle)
{
    CDBBatch batch(*this);

    std::pair<char, uint256> key = std::make_pair(withdrawalBundle.sidechainop, withdrawalBundle.GetID());
    batch.Write(key, withdrawalBundle);

    // Also index the WithdrawalBundle by the WithdrawalBundle transaction hash
    uint256 hashWithdrawalBundle = withdrawalBundle.tx.GetHash();
    std::pair<char, uint256> keyTx = std::make_pair(DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP, hashWithdrawalBundle);
    batch.Write(keyTx, withdrawalBundle);

    // Also write withdrawal status updates if WithdrawalBundle status changes
    std::vector<SidechainWithdrawal> vUpdate;
    for (const uint256& id: withdrawalBundle.vWithdrawalID) {
        SidechainWithdrawal withdrawal;
        if (!GetWithdrawal(id, withdrawal)) {
            LogPrintf("%s: Failed to read withdrawal of WithdrawalBundle from LDB!\n", __func__);
            return false;
        }
        if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_FAILED) {
            withdrawal.status = WITHDRAWAL_UNSPENT;
            vUpdate.push_back(withdrawal);
        }
        else
        if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_SPENT) {
            withdrawal.status = WITHDRAWAL_SPENT;
            vUpdate.push_back(withdrawal);
        }
        else
        if (withdrawalBundle.status == WITHDRAWAL_BUNDLE_CREATED) {
            withdrawal.status = WITHDRAWAL_IN_BUNDLE;
            vUpdate.push_back(withdrawal);
        }
    }

    if (!WriteWithdrawalUpdate(vUpdate)) {
        LogPrintf("%s: Failed to write withdrawal update!\n", __func__);
        return false;
    }

    return WriteBatch(batch, true);
}

bool CSidechainTreeDB::WriteLastWithdrawalBundleHash(const uint256& hash)
{
    return Write(DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE, hash);
}

bool CSidechainTreeDB::GetWithdrawal(const uint256& objid, SidechainWithdrawal& withdrawal)
{
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_WITHDRAWAL_OP, objid), withdrawal))
        return true;

    return false;
}

bool CSidechainTreeDB::GetWithdrawalBundle(const uint256& objid, SidechainWithdrawalBundle& withdrawalBundle)
{
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP, objid), withdrawalBundle))
        return true;

    return false;
}

bool CSidechainTreeDB::GetDeposit(const uint256& objid, SidechainDeposit& deposit)
{
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_DEPOSIT_OP, objid), deposit))
        return true;

    return false;
}

std::vector<SidechainWithdrawal> CSidechainTreeDB::GetWithdrawals(const uint8_t& nSidechain)
{
    const char sidechainop = DB_SIDECHAIN_WITHDRAWAL_OP;
    std::ostringstream ss;
    ::Serialize(ss, std::make_pair(std::make_pair(sidechainop, nSidechain), uint256()));

    std::vector<SidechainWithdrawal> vWT;

    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(ss.str());
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();

        std::pair<char, uint256> key;
        SidechainWithdrawal wt;
        if (pcursor->GetKey(key) && key.first == sidechainop) {
            if (pcursor->GetSidechainValue(wt))
                vWT.push_back(wt);
        }

        pcursor->Next();
    }

    return vWT;
}

std::vector<SidechainWithdrawalBundle> CSidechainTreeDB::GetWithdrawalBundles(const uint8_t& nSidechain)
{
    const char sidechainop = DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP;
    std::ostringstream ss;
    ::Serialize(ss, std::make_pair(std::make_pair(sidechainop, nSidechain), uint256()));

    std::vector<SidechainWithdrawalBundle> vWithdrawalBundle;

    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(ss.str());
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();

        std::pair<char, uint256> key;
        SidechainWithdrawalBundle withdrawalBundle;
        if (pcursor->GetKey(key) && key.first == sidechainop) {
            if (pcursor->GetSidechainValue(withdrawalBundle)) {
                // Only return the WithdrawalBundle(s) indexed by ID
                if (key.second == withdrawalBundle.GetID())
                    vWithdrawalBundle.push_back(withdrawalBundle);
            }
        }

        pcursor->Next();
    }
    return vWithdrawalBundle;
}

std::vector<SidechainDeposit> CSidechainTreeDB::GetDeposits(const uint8_t& nSidechain)
{
    const char sidechainop = DB_SIDECHAIN_DEPOSIT_OP;
    std::ostringstream ss;
    ::Serialize(ss, std::make_pair(std::make_pair(sidechainop, nSidechain), uint256()));

    std::vector<SidechainDeposit> vDeposit;

    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(ss.str());
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();

        std::pair<char, uint256> key;
        SidechainDeposit deposit;
        if (pcursor->GetKey(key) && key.first == sidechainop) {
            if (pcursor->GetSidechainValue(deposit))
                // Only return the deposits(s) indexed by ID
                if (key.second == deposit.GetID())
                    vDeposit.push_back(deposit);
        }

        pcursor->Next();
    }
    return vDeposit;
}

bool CSidechainTreeDB::HaveDeposits()
{
    const char sidechainop = DB_SIDECHAIN_DEPOSIT_OP;
    std::ostringstream ss;
    ::Serialize(ss, std::make_pair(std::make_pair(sidechainop, DB_SIDECHAIN_DEPOSIT_OP), uint256()));

    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(ss.str());
    if (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        SidechainDeposit d;
        if (pcursor->GetKey(key) && key.first == sidechainop) {
            if (pcursor->GetSidechainValue(d))
                return true;
        }
    }
    return false;
}

bool CSidechainTreeDB::HaveDepositNonAmount(const uint256& hashNonAmount)
{
    SidechainDeposit deposit;
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_DEPOSIT_OP, hashNonAmount),
                deposit))
        return true;

    return false;
}

bool CSidechainTreeDB::GetLastDeposit(SidechainDeposit& deposit)
{
    // Look up the last deposit non amount hash
    uint256 objid;
    if (!Read(DB_LAST_SIDECHAIN_DEPOSIT, objid))
        return false;

    // Read the last deposit
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_DEPOSIT_OP, objid), deposit))
        return true;

    return false;
}

bool CSidechainTreeDB::GetLastWithdrawalBundleHash(uint256& hash)
{
    // Look up the last deposit non amount hash
    if (!Read(DB_LAST_SIDECHAIN_WITHDRAWAL_BUNDLE, hash))
        return false;

    return true;
}

bool CSidechainTreeDB::HaveWithdrawalBundle(const uint256& hashWithdrawalBundle) const
{
    SidechainWithdrawalBundle withdrawalBundle;
    if (ReadSidechain(std::make_pair(DB_SIDECHAIN_WITHDRAWAL_BUNDLE_OP, hashWithdrawalBundle), withdrawalBundle))
        return true;

    return false;
}

BitAssetDB::BitAssetDB(size_t nCacheSize, bool fMemory, bool fWipe)
    : CDBWrapper(GetDataDir() / "blocks" / "BitAssets", nCacheSize, fMemory, fWipe) { }

bool BitAssetDB::WriteBitAssets(const std::vector<BitAsset>& vAsset)
{
    CDBBatch batch(*this);
    for (const BitAsset& asset : vAsset) {
        std::pair<char, uint32_t> key = std::make_pair(DB_ASSET, asset.nID);
        batch.Write(key, asset);
    }
    return WriteBatch(batch, true);
}

std::vector<BitAsset> BitAssetDB::GetAssets()
{
    std::ostringstream ss;
    ::Serialize(ss, std::make_pair(DB_ASSET, 0));

    std::vector<BitAsset> vAsset;

    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(ss.str());
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();

        std::pair<char, uint32_t> key;
        BitAsset asset;
        if (pcursor->GetKey(key) && key.first == DB_ASSET) {
            if (pcursor->GetValue(asset))
                vAsset.push_back(asset);
        }

        pcursor->Next();
    }
    return vAsset;
}

bool BitAssetDB::GetLastAssetID(uint32_t& nID)
{
    // Look up the last asset ID (in chronological order)
    if (!Read(DB_ASSET_LAST_ID, nID))
        return false;

    return true;
}

bool BitAssetDB::WriteLastAssetID(const uint32_t nID)
{
    return Write(DB_ASSET_LAST_ID, nID);
}

bool BitAssetDB::RemoveAsset(const uint32_t nID)
{
    std::pair<char, uint32_t> key = std::make_pair(DB_ASSET, nID);
    return Erase(key);
}

bool BitAssetDB::GetAsset(const uint32_t nID, BitAsset& asset)
{
    return Read(std::make_pair(DB_ASSET, nID), asset);
}

namespace {

//! Legacy class to deserialize pre-pertxout database entries without reindex.
class CCoins
{
public:
    //! whether transaction is a coinbase
    bool fCoinBase;

    //! unspent transaction outputs; spent outputs are .IsNull(); spent outputs at the end of the array are dropped
    std::vector<CTxOut> vout;

    //! at which height this transaction was included in the active block chain
    int nHeight;

    //! empty constructor
    CCoins() : fCoinBase(false), vout(0), nHeight(0) { }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        // version
        int nVersionDummy;
        ::Unserialize(s, VARINT(nVersionDummy));
        // header code
        ::Unserialize(s, VARINT(nCode));
        fCoinBase = nCode & 1;
        std::vector<bool> vAvail(2, false);
        vAvail[0] = (nCode & 2) != 0;
        vAvail[1] = (nCode & 4) != 0;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }
        // txouts themself
        vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, REF(CTxOutCompressor(vout[i])));
        }
        // coinbase height
        ::Unserialize(s, VARINT(nHeight));
    }
};

}

/** Upgrade the database from older formats.
 *
 * Currently implemented: from the per-tx utxo model (0.8..0.14.x) to per-txout.
 */
bool CCoinsViewDB::Upgrade() {
    std::unique_ptr<CDBIterator> pcursor(db.NewIterator());
    pcursor->Seek(std::make_pair(DB_COINS, uint256()));
    if (!pcursor->Valid()) {
        return true;
    }

    int64_t count = 0;
    LogPrintf("Upgrading utxo-set database...\n");
    LogPrintf("[0%%]...");
    uiInterface.ShowProgress(_("Upgrading UTXO database"), 0, true);
    size_t batch_size = 1 << 24;
    CDBBatch batch(db);
    int reportDone = 0;
    std::pair<unsigned char, uint256> key;
    std::pair<unsigned char, uint256> prev_key = {DB_COINS, uint256()};
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        if (ShutdownRequested()) {
            break;
        }
        if (pcursor->GetKey(key) && key.first == DB_COINS) {
            if (count++ % 256 == 0) {
                uint32_t high = 0x100 * *key.second.begin() + *(key.second.begin() + 1);
                int percentageDone = (int)(high * 100.0 / 65536.0 + 0.5);
                uiInterface.ShowProgress(_("Upgrading UTXO database"), percentageDone, true);
                if (reportDone < percentageDone/10) {
                    // report max. every 10% step
                    LogPrintf("[%d%%]...", percentageDone);
                    reportDone = percentageDone/10;
                }
            }
            CCoins old_coins;
            if (!pcursor->GetValue(old_coins)) {
                return error("%s: cannot parse CCoins record", __func__);
            }
            COutPoint outpoint(key.second, 0);
            for (size_t i = 0; i < old_coins.vout.size(); ++i) {
                if (!old_coins.vout[i].IsNull() && !old_coins.vout[i].scriptPubKey.IsUnspendable()) {
                    Coin newcoin(std::move(old_coins.vout[i]), old_coins.nHeight, old_coins.fCoinBase, false, false, 0);
                    outpoint.n = i;
                    CoinEntry entry(&outpoint);
                    batch.Write(entry, newcoin);
                }
            }
            batch.Erase(key);
            if (batch.SizeEstimate() > batch_size) {
                db.WriteBatch(batch);
                batch.Clear();
                db.CompactRange(prev_key, key);
                prev_key = key;
            }
            pcursor->Next();
        } else {
            break;
        }
    }
    db.WriteBatch(batch);
    db.CompactRange({DB_COINS, uint256()}, key);
    uiInterface.ShowProgress("", 100, false);
    LogPrintf("[%s].\n", ShutdownRequested() ? "CANCELLED" : "DONE");
    return !ShutdownRequested();
}
