#ifndef ADRESSBOOKDIALOG_H
#define ADRESSBOOKDIALOG_H

#include <QDialog>

namespace Ui {
class AdressBookDialog;
}

class AdressBookDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdressBookDialog(QWidget *parent = nullptr);
    ~AdressBookDialog();
private slots:
    void onButtonNew();

private:
    Ui::AdressBookDialog *ui;
    QPushButton* m_buttonNew;

};

#endif // ADRESSBOOKDIALOG_H
