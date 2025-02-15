#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "senddialog.h"
#include "coinminerthread.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    OnInIt();
}

MainWindow::~MainWindow()
{
    delete ui;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();  // Ensure this line is present and the window is shown
    return a.exec();  // Starts the event loop
}

void MainWindow::on_pushButton_clicked()
{
    SendDialog *dialog = new SendDialog(this);
    dialog->show();
}

void MainWindow::on_btnGenerateCoin_clicked()
{
    CoinMinerThread *thread = new CoinMinerThread(this);
    thread->start();

}
using namespace TinyTokenizer;

void MainWindow::OnInIt()
{

    // Parse command-line arguments
    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "/datadir") {
            strSetDataDir = args[++i].toStdString();
        } else if (args[i] == "/proxy") {
            addrProxy = CAddress(args[++i].toStdString().c_str());
        } else if (args[i] == "/debug") {
            fDebug = true;
        }
        // Add more argument handling as needed
    }

    // Debug printing
    qDebug() << "Loading addresses...";
    if (!LoadAddresses()) {
        qDebug() << "Error loading addr.dat";
        return;
    }

    qDebug() << "Loading block index...";
    if (!LoadBlockIndex()) {
        qDebug() << "Error loading blkindex.dat";
        return;
    }

    qDebug() << "Loading wallet..."; //now we are here
    if (!LoadWallet()) {
        qDebug() << "Error loading wallet.dat";
        return;
    }

    qDebug() << "Loading GPT2 model...";
    if (!LoadGPT2()) {
        qDebug() << "Error loading gpt2 model";
        return;
    }

    qDebug() << "Loading GPT2 Tokenizer..."; //now we are here
    if (!LoadGP2Tokenizer()) {
        qDebug() << "Error loading gpt2 tokenizer";
        return;
    }


    qDebug() << "Loading GPT2 Data Loader..."; //now we are here
    if (!LoadGP2DataLoader()) {
        qDebug() << "Error loading gpt2 data loader";
        return;
    }

    // If there are command-line arguments like "/min", minimize the window
    if (args.contains("/min")) {
        this->setWindowState(Qt::WindowMinimized);
    }


    // If generating coins, start the mining thread
    // if (fGenerateCoins) {
    //     CoinMinerThread *minerThread = new CoinMinerThread(this);
    //     minerThread->start();
    // }
}



void MainWindow::on_minerTool_clicked()
{
    MinerTool();
}

