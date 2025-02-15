#include "adressbookdialog.h"
#include "ui_adressbookdialog.h"

AdressBookDialog::AdressBookDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AdressBookDialog)
{
    ui->setupUi(this);
}

AdressBookDialog::~AdressBookDialog()
{
    delete ui;
}

void AdressBookDialog::onButtonNew()
{
    // // Ask name
    // string strName;
    // string strAddress;
    // do
    // {
    //     CGetTextFromUserDialog dialog(this, "New Address", "Name", strName, "Address", strAddress);
    //     if (!dialog.ShowModal())
    //         return;
    //     strName = dialog.GetValue1();
    //     strAddress = dialog.GetValue2();
    // }
    // while (CheckIfMine(strAddress, "New Address"));

    // // Add to list and select it
    // SetAddressBookName(strAddress, strName);
    // int nIndex = InsertLine(m_listCtrl, strName, strAddress);
    // SetSelection(m_listCtrl, nIndex);
    // m_listCtrl->SetFocus();
    // pframeMain->RefreshListCtrl();
}
