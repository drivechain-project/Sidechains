#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Tests that a block is rejected when the coinbase transaction
has *multiple* commitments for a single drivechain
"""
from test_framework.mininode import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import time
from test_framework.blocktools import create_block, create_coinbase
from test_framework.script import CScript, OP_RETURN, OP_0
class InvalidDrivechainCommitmentTest(BitcoinTestFramework):
    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("BITCOIND", "bitcoind"),
                          help="bitcoind binary to test")

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self):
        # Node0 will be used to test behavior of processing unrequested blocks
        # from peers which are not whitelisted, while Node1 will be used for
        # the whitelisted case.
        self.setup_nodes()
        connect_nodes(self.nodes[0],1)
        self.sync_all([self.nodes])
    def run_test(self):
        # Setup the p2p connections and start up the network thread.
        #NetworkThread().start() # Start up network handling in another thread
        node0 = NodeConnCB()
        connections = []
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], node0))
        connections.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[1], node0))
        node0.add_connection(connections[0])
        node0.add_connection(connections[1])
        # Start up network handling in another thread. This needs to be called
        # after the P2P connections have been created.
        NetworkThread().start()
        # wait_for_verack ensures that the P2P connection is fully up.
        node0.wait_for_verack()
         
        h = self.nodes[0].generate(1)[0]
        self.sync_all([self.nodes])
        assert(self.nodes[0].getblockcount() == 1)
        assert(self.nodes[1].getblockcount() == 1)
        
	#TODO: figure out a way to generate a random hash instead of using the getbestblockhash
        drivechain_block_hash = self.nodes[0].getbestblockhash()
        chain_commitment = CScript([OP_RETURN, bytearray.fromhex(drivechain_block_hash), OP_0])
        #This creates two commitments for the same drivechain -- this is invalid so the block should not be accepted
        chain_commitments = [chain_commitment, chain_commitment]
        hashPrev = self.nodes[0].getbestblockhash()
        block = create_block(int("0x" + hashPrev, 0), create_coinbase(2,None,chain_commitments))
        block.solve()
        #Is this propogating the block correctly????
        node0.send_message(msg_block(block))
        self.log.info("block.hash: " + str(block.hash))
        assert(self.nodes[0].getbestblockhash() != block.hash)
        assert(self.nodes[1].getbestblockhash() != block.hash)
        assert(self.nodes[0].getbestblockhash() == self.nodes[1].getbestblockhash())
        [ c.disconnect_node() for c in connections ]

if __name__ == '__main__':
    InvalidDrivechainCommitmentTest().main()
