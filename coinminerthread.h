#ifndef COINMINERTHREAD_H
#define COINMINERTHREAD_H
#include <QThread>
#include <QDebug>

class CoinMinerThread : public QThread
{
    Q_OBJECT

public:
    explicit CoinMinerThread(QObject *parent = nullptr);
    void run() override; // The main function that gets executed when the thread is started

private:
    bool CoinMinerT();
};
#endif // COINMINERTHREAD_H
