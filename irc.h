#ifndef IRC_H
#define IRC_H
// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

extern bool RecvLine(SOCKET hSocket, string& strLine);
extern void ThreadIRCSeed(void* parg);
extern bool fRestartIRCSeed;

extern map<vector<unsigned char>, CAddress> mapIRCAddresses;
extern CCriticalSection cs_mapIRCAddresses;

#endif // IRC_H
