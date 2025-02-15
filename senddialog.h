#ifndef SENDDIALOG_H
#define SENDDIALOG_H

#include <QDialog>
#include <QIcon>
#include <QClipboard>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

namespace Ui {
class SendDialog;
}

class SendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendDialog(QWidget *parent = nullptr);
    ~SendDialog();

private slots:
    // void onTextAddress();
    // void onKillFocusAmount();
    // void onButtonAddressBook();
    // void onButtonPaste();
    void onButtonSend();
    // void onButtonCancel();

private:
    QLineEdit* m_textCtrlAddress;
    QLineEdit* m_textCtrlAmount;
    QLabel* m_bitmapCheckMark;
    QPushButton* m_buttonPaste;
    QPushButton* m_buttonAddressBook;
    QPushButton* m_buttonSend;
    QPushButton* m_buttonCancel;
    Ui::SendDialog *ui;
};

#endif // SENDDIALOG_H
