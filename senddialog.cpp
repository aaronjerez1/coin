#include "senddialog.h"
#include "ui_senddialog.h"

SendDialog::SendDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SendDialog)
{
    ui->setupUi(this);
    // Set the address field
    m_textCtrlAddress = new QLineEdit(this);
    m_textCtrlAddress->setText("strAddress");

    // Setup UI components (simplified)
    // m_buttonPaste = new QPushButton("Paste", this);
    // m_buttonAddressBook = new QPushButton("Address Book", this);
    m_buttonSend = new QPushButton("Send", this);
    // m_buttonCancel = new QPushButton("Cancel", this);

    // Connect signals to slots
    // connect(m_buttonPaste, &QPushButton::clicked, this, &SendDialog::onButtonPaste);
    // connect(m_buttonAddressBook, &QPushButton::clicked, this, &SendDialog::onButtonAddressBook);
    connect(m_buttonSend, &QPushButton::clicked, this, &SendDialog::onButtonSend);
    // connect(m_buttonCancel, &QPushButton::clicked, this, &SendDialog::onButtonCancel);
    // connect(m_textCtrlAddress, &QLineEdit::textChanged, this, &SendDialog::onTextAddress);
    // connect(m_textCtrlAmount, &QLineEdit::editingFinished, this, &SendDialog::onKillFocusAmount);

    // Set an icon for the window
    //setWindowIcon(QIcon(":/icons/send16.png"));

    // Set the tab order
    // setTabOrder(m_buttonPaste, m_buttonCancel);
    // setTabOrder(m_buttonAddressBook, m_buttonPaste);

    // Set layout (simplified)
    // Layout and other initialization would be handled here...
}

SendDialog::~SendDialog()
{
    delete ui;
}

// void SendDialog::onTextAddress()
// {
//     // Check if the address is valid and update UI accordingly
//     bool fCoinAddress = IsValidCoinAddress(m_textCtrlAddress->text().toStdString());
//     m_bitmapCheckMark->setVisible(fCoinAddress);
// }

// void SendDialog::onKillFocusAmount()
// {
//     // Handle focus out event for amount field
//     if (!m_textCtrlAmount->text().trimmed().isEmpty()) {
//         int64 nTmp;
//         if (ParseMoney(m_textCtrlAmount->text(), nTmp)) {
//             m_textCtrlAmount->setText(FormatMoney(nTmp));
//         }
//     }
// }

// void SendDialog::onButtonPaste()
// {
//     // Paste clipboard text into the address field
//     QClipboard *clipboard = QApplication::clipboard();
//     m_textCtrlAddress->setText(clipboard->text());
// }

// m_textCtrlAmount
// nTransactionFee
void SendDialog::onButtonSend()
{
    // Handle the logic for sending coins
    int64 nValue = 0;
    //int64 nTransactionFee = 0;

    // Store the result of toStdString() in a local variable to ensure its lifetime
    //std::string amountStr = m_textCtrlAmount->text().toStdString();
    //qDebug() << "Converted amount: " << QString::fromStdString(amountStr);

    // Now safely use amountStr.c_str() with ParseMoney
    if (!ParseMoney("amountStr.c_str()", nValue) || nValue <= 0) {
        QMessageBox::warning(this, tr("Send Coins"), tr("Error in amount"));
        return;
    }

    // More transaction logic here...
}


// void SendDialog::onButtonCancel()
// {
//     close();
// }
