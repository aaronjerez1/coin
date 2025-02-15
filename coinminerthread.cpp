#include "coinminerthread.h"

CoinMinerThread::CoinMinerThread(QObject *parent)
    : QThread(parent) {}

void CoinMinerThread::run()
{
    qDebug() << "Mining started!";
    CoinMinerT(); // Replace with your actual mining logic
    qDebug() << "Mining finished!";
}

bool CoinMinerThread::CoinMinerT() {
    qDebug() << "CoinMiner started";
    return CoinMiner();
}
