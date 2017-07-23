// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "guldensendcoinsentry.h"
#include "_Gulden/forms/ui_guldensendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "accounttablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <QApplication>
#include <QClipboard>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include "GuldenGUI.h"

GuldenSendCoinsEntry::GuldenSendCoinsEntry(const PlatformStyle* platformStyle, QWidget* parent)
    : QFrame(parent)
    , ui(new Ui::GuldenSendCoinsEntry)
    , model(0)
    , platformStyle(platformStyle)
{
    ui->setupUi(this);

    QList<QTabBar*> tabBar = this->ui->sendCoinsRecipientBook->findChildren<QTabBar*>();
    tabBar.at(0)->setCursor(Qt::PointingHandCursor);

    ui->addressBookTabTable->horizontalHeader()->setStretchLastSection(true);
    ui->addressBookTabTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->addressBookTabTable->horizontalHeader()->hide();

    ui->myAccountsTabTable->horizontalHeader()->setStretchLastSection(true);
    ui->myAccountsTabTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->myAccountsTabTable->horizontalHeader()->hide();

    ui->searchLabel1->setContentsMargins(0, 0, 0, 0);
    ui->searchLabel2->setContentsMargins(0, 0, 0, 0);
    ui->addressBookTabTable->setContentsMargins(0, 0, 0, 0);
    ui->myAccountsTabTable->setContentsMargins(0, 0, 0, 0);

    connect(ui->searchBox1, SIGNAL(textEdited(QString)), this, SLOT(searchChangedAddressBook(QString)));
    connect(ui->searchBox2, SIGNAL(textEdited(QString)), this, SLOT(searchChangedMyAccounts(QString)));

    connect(ui->sendCoinsRecipientBook, SIGNAL(currentChanged(int)), this, SIGNAL(sendTabChanged()));
    connect(ui->sendCoinsRecipientBook, SIGNAL(currentChanged(int)), this, SLOT(tabChanged()));
    connect(ui->receivingAddress, SIGNAL(textEdited(QString)), this, SLOT(addressChanged()));

    ui->receivingAddress->setProperty("valid", true);
}

GuldenSendCoinsEntry::~GuldenSendCoinsEntry()
{
    delete ui;
}

void GuldenSendCoinsEntry::on_pasteButton_clicked()
{
}

void GuldenSendCoinsEntry::on_addressBookButton_clicked()
{
    /*if(!model)
        return;
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }*/
}

void GuldenSendCoinsEntry::on_payTo_textChanged(const QString& address)
{
    updateLabel(address);
}

void GuldenSendCoinsEntry::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel()) {
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        ui->payAmount->setCurrency(model->getOptionsModel(), model->getOptionsModel()->getTicker(), BitcoinAmountField::AmountFieldCurrency::CurrencyGulden);
    }

    if (model) {
        {
            QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
            proxyModel->setSourceModel(model->getAddressTableModel());
            proxyModel->setDynamicSortFilter(true);
            proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModel->setFilterRole(AddressTableModel::TypeRole);
            proxyModel->setFilterFixedString(AddressTableModel::Send);

            proxyModelRecipients = new QSortFilterProxyModel(this);
            proxyModelRecipients->setSourceModel(proxyModel);
            proxyModelRecipients->setDynamicSortFilter(true);
            proxyModelRecipients->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModelRecipients->setFilterFixedString("");
            proxyModelRecipients->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModelRecipients->setFilterKeyColumn(AddressTableModel::ColumnIndex::Label);

            ui->addressBookTabTable->setModel(proxyModelRecipients);
        }
        {
            QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
            proxyModel->setSourceModel(model->getAccountTableModel());
            proxyModel->setDynamicSortFilter(true);
            proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModel->setFilterRole(AccountTableModel::TypeRole);
            proxyModel->setFilterFixedString(AccountTableModel::Normal);

            QSortFilterProxyModel* proxyInactive = new QSortFilterProxyModel(this);
            proxyInactive->setSourceModel(proxyModel);
            proxyInactive->setDynamicSortFilter(true);
            proxyInactive->setFilterRole(AccountTableModel::ActiveAccountRole);
            proxyInactive->setFilterFixedString(AccountTableModel::Inactive);

            proxyModelAddresses = new QSortFilterProxyModel(this);
            proxyModelAddresses->setSourceModel(proxyInactive);
            proxyModelAddresses->setDynamicSortFilter(true);
            proxyModelAddresses->setSortCaseSensitivity(Qt::CaseInsensitive);
            proxyModelAddresses->setFilterFixedString("");
            proxyModelAddresses->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModelAddresses->setFilterKeyColumn(AddressTableModel::ColumnIndex::Label);
            proxyModelAddresses->setSortRole(Qt::DisplayRole);
            proxyModelAddresses->sort(0);

            connect(proxyModel, SIGNAL(rowsInserted(QModelIndex, int, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(rowsRemoved(QModelIndex, int, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(columnsInserted(QModelIndex, int, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(columnsRemoved(QModelIndex, int, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(columnsMoved(QModelIndex, int, int, QModelIndex, int)), proxyModelAddresses, SLOT(invalidate()));
            connect(proxyModel, SIGNAL(modelReset()), proxyModelAddresses, SLOT(invalidate()));

            ui->myAccountsTabTable->setModel(proxyModelAddresses);
        }
        connect(ui->addressBookTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SIGNAL(sendTabChanged()));
        connect(ui->myAccountsTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SIGNAL(sendTabChanged()));

        connect(ui->addressBookTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(addressBookSelectionChanged()));
        connect(ui->myAccountsTabTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SIGNAL(myAccountsSelectionChanged()));
    }

    clear();
}

void GuldenSendCoinsEntry::addressChanged()
{
    SendCoinsRecipient val = getValue(false);
    if (val.paymentType == SendCoinsRecipient::PaymentType::InvalidPayment) {
    } else {
        ui->receivingAddress->setProperty("valid", true);

        if (val.paymentType == SendCoinsRecipient::PaymentType::BCOINPayment) {
            ui->payAmount->setCurrency(NULL, NULL, BitcoinAmountField::AmountFieldCurrency::CurrencyBCOIN);
        } else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment) {
            ui->payAmount->setCurrency(NULL, NULL, BitcoinAmountField::AmountFieldCurrency::CurrencyEuro);
        } else {
            ui->payAmount->setCurrency(NULL, NULL, BitcoinAmountField::AmountFieldCurrency::CurrencyGulden);
        }
    }
}

void GuldenSendCoinsEntry::tabChanged()
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0: {
        addressChanged();
    } break;
    case 1: {
        addressChanged();
    } break;
    case 2: {
        ui->payAmount->setCurrency(NULL, NULL, BitcoinAmountField::AmountFieldCurrency::CurrencyGulden);
    } break;
    }
}

void GuldenSendCoinsEntry::addressBookSelectionChanged()
{
    tabChanged();
}

void GuldenSendCoinsEntry::myAccountsSelectionChanged()
{
    tabChanged();
}

void GuldenSendCoinsEntry::clear()
{
    ui->receivingAddress->setProperty("valid", true);
    ui->receivingAddress->setText("");
    ui->payAmount->clear();

    /*ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();

    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();

    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();*/

    updateDisplayUnit();
}

void GuldenSendCoinsEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

bool GuldenSendCoinsEntry::validate()
{
    if (!model)
        return false;

    bool retval = true;

    if (recipient.paymentRequest.IsInitialized())
        return retval;

    SendCoinsRecipient val = getValue(false);
    if (val.paymentType == SendCoinsRecipient::PaymentType::InvalidPayment) {
        ui->receivingAddress->setProperty("valid", false);
        retval = false;
    } else {
        ui->receivingAddress->setProperty("valid", true);

        if (val.paymentType == SendCoinsRecipient::PaymentType::BCOINPayment) {

            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-BTC");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-BTC");
            if (ui->payAmount->valueForCurrency(0) > currencyMax || ui->payAmount->valueForCurrency(0) < currencyMin) {
                ui->payAmount->setValid(false);
                return false;
            }
        } else if (val.paymentType == SendCoinsRecipient::PaymentType::IBANPayment) {

            CAmount currencyMax = model->getOptionsModel()->getNocksSettings()->getMaximumForCurrency("NLG-EUR");
            CAmount currencyMin = model->getOptionsModel()->getNocksSettings()->getMinimumForCurrency("NLG-EUR");
            if (ui->payAmount->valueForCurrency(0) > currencyMax || ui->payAmount->valueForCurrency(0) < currencyMin) {
                ui->payAmount->setValid(false);
                return false;
            }
        } else {
        }
    }

    if (!ui->payAmount->validate()) {
        retval = false;
    }

    if (ui->payAmount->valueForCurrency(0) <= 0) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    /*if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }*/

    return retval;
}

SendCoinsRecipient::PaymentType GuldenSendCoinsEntry::getPaymentType(const QString& fromAddress)
{
    SendCoinsRecipient::PaymentType ret = SendCoinsRecipient::PaymentType::InvalidPayment;
    if (model->validateAddress(recipient.address)) {
        ret = SendCoinsRecipient::PaymentType::NormalPayment;
    } else {
        QString compareModified = recipient.address;
        if (model->validateAddressBCOIN(compareModified)) {
            ret = SendCoinsRecipient::PaymentType::BCOINPayment;
        } else {

            if (model->validateAddressIBAN(recipient.address)) {
                ret = SendCoinsRecipient::PaymentType::IBANPayment;
            }
        }
    }
    return ret;
}

SendCoinsRecipient GuldenSendCoinsEntry::getValue(bool showWarningDialogs)
{

    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    recipient.addToAddressBook = false;
    recipient.fSubtractFeeFromAmount = false;
    recipient.amount = ui->payAmount->valueForCurrency();

    if (recipient.amount >= (pwalletMain->GetBalance(model->getActiveAccount(), true) + pwalletMain->GetUnconfirmedBalance(model->getActiveAccount(), true))) {
        if (showWarningDialogs) {
            QString message = tr("The amount you want to send exceeds your balance, amount has been automatically adjusted downwards to match your balance. Please ensure this is what you want before proceeding to avoid short payment of your recipient.");
            QDialog* d = GuldenGUI::createDialog(this, message, tr("Okay"), "", 400, 180);
            d->exec();
        }

        recipient.amount = pwalletMain->GetBalance(model->getActiveAccount(), true) + pwalletMain->GetUnconfirmedBalance(model->getActiveAccount(), true);
        recipient.fSubtractFeeFromAmount = true;
    }

    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0: {
        recipient.address = ui->receivingAddress->text();
        recipient.label = ui->receivingAddressLabel->text();
        recipient.addToAddressBook = ui->checkBoxAddToAddressBook->isChecked();
        break;
    }
    case 1: {
        if (proxyModelRecipients) {
            QModelIndexList selection = ui->addressBookTabTable->selectionModel()->selectedRows();
            if (selection.count() > 0) {
                QModelIndex index = selection.at(0);
                recipient.address = index.sibling(index.row(), 1).data(Qt::DisplayRole).toString();
                recipient.label = index.sibling(index.row(), 0).data(Qt::DisplayRole).toString();
            }
        }
        break;
    }
    case 2: {
        if (proxyModelAddresses) {
            QModelIndexList selection = ui->myAccountsTabTable->selectionModel()->selectedRows();
            if (selection.count() > 0) {
                QModelIndex index = selection.at(0);
                QString sAccountUUID = index.data(AccountTableModel::AccountTableRoles::SelectedAccountRole).toString();

                LOCK(pwalletMain->cs_wallet);

                CReserveKey key(pwalletMain, pwalletMain->mapAccounts[sAccountUUID.toStdString()], KEYCHAIN_EXTERNAL);
                CPubKey pubKey;
                if (!key.GetReservedKey(pubKey)) {

                    recipient.paymentType = SendCoinsRecipient::PaymentType::InvalidPayment;
                    recipient.address = QString("error");
                    return recipient;
                }
                key.KeepKey();
                CKeyID keyID = pubKey.GetID();
                recipient.address = QString::fromStdString(CBitcoinAddress(keyID).ToString());
                recipient.label = QString::fromStdString(pwalletMain->mapAccountLabels[sAccountUUID.toStdString()]);
            }
        }
        break;
    }
    }

    recipient.address.replace(QRegularExpression("\\s"), "");

    recipient.address.replace(QRegularExpression("\\p{P}"), "");

    recipient.paymentType = getPaymentType(recipient.address);

    return recipient;
}

QWidget* GuldenSendCoinsEntry::setupTabChain(QWidget* prev)
{

    return ui->payAmount;
}

void GuldenSendCoinsEntry::setValue(const SendCoinsRecipient& value)
{
    recipient = value;

    /*if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    {

        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }*/
}

void GuldenSendCoinsEntry::setAddress(const QString& address)
{
    /*ui->payTo->setText(address);
    ui->payAmount->setFocus();*/
}

bool GuldenSendCoinsEntry::isClear()
{

    return true;
}

void GuldenSendCoinsEntry::setFocus()
{
}

bool GuldenSendCoinsEntry::ShouldShowEditButton() const
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0:
    case 2:
        return false;
    }
    if (ui->addressBookTabTable->currentIndex().row() >= 0)
        return true;
    return false;
}

bool GuldenSendCoinsEntry::ShouldShowClearButton() const
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 1:
    case 2:
        return false;
    }
    return true;
}

bool GuldenSendCoinsEntry::ShouldShowDeleteButton() const
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0:
    case 2:
        return false;
    }
    if (ui->addressBookTabTable->currentIndex().row() >= 0)
        return true;
    return false;
}

void GuldenSendCoinsEntry::deleteAddressBookEntry()
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0:
    case 2:
        return;
    }
    if (proxyModelRecipients && ui->addressBookTabTable->currentIndex().row() >= 0) {
        QModelIndexList indexes = ui->addressBookTabTable->selectionModel()->selectedRows();
        if (!indexes.isEmpty()) {
            QString message = tr("Are you sure you want to delete %1 from the address book?").arg(indexes.at(0).sibling(indexes.at(0).row(), 0).data(Qt::DisplayRole).toString());
            QDialog* d = GuldenGUI::createDialog(this, message, tr("Delete"), tr("Cancel"), 400, 180);

            int result = d->exec();
            if (result == QDialog::Accepted) {
                ui->addressBookTabTable->model()->removeRow(indexes.at(0).row());
            }
        }
    }
}

void GuldenSendCoinsEntry::editAddressBookEntry()
{
    switch (ui->sendCoinsRecipientBook->currentIndex()) {
    case 0:
    case 2:
        return;
    }
    if (proxyModelRecipients && ui->addressBookTabTable->currentIndex().row() >= 0) {
        QModelIndexList indexes = ui->addressBookTabTable->selectionModel()->selectedRows();
        if (!indexes.isEmpty()) {
            QDialog* d = new QDialog(this);
            d->setMinimumSize(QSize(400, 200));
            QVBoxLayout* vbox = new QVBoxLayout();
            vbox->setSpacing(0);
            vbox->setContentsMargins(0, 0, 0, 0);

            QLineEdit* lineEditAddress = new QLineEdit(d);
            vbox->addWidget(lineEditAddress);
            lineEditAddress->setText(indexes.at(0).sibling(indexes.at(0).row(), 0).data(Qt::DisplayRole).toString());
            lineEditAddress->setObjectName("receivingAddress_dialog");
            lineEditAddress->setContentsMargins(0, 0, 0, 0);

            QLineEdit* lineEditLabel = new QLineEdit(d);
            vbox->addWidget(lineEditLabel);
            lineEditLabel->setText(indexes.at(0).sibling(indexes.at(0).row(), 1).data(Qt::DisplayRole).toString());
            lineEditLabel->setObjectName("receivingAddressLabel_dialog");
            lineEditLabel->setContentsMargins(0, 0, 0, 0);

            QWidget* spacer = new QWidget(d);
            spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            vbox->addWidget(spacer);

            QFrame* horizontalLine = new QFrame(d);
            horizontalLine->setFrameStyle(QFrame::HLine);
            horizontalLine->setFixedHeight(1);
            horizontalLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            horizontalLine->setStyleSheet(GULDEN_DIALOG_HLINE_STYLE);
            vbox->addWidget(horizontalLine);

            QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Reset, d);
            QObject::connect(buttonBox, SIGNAL(accepted()), d, SLOT(accept()));
            QObject::connect(buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), d, SLOT(reject()));
            vbox->addWidget(buttonBox);
            buttonBox->setContentsMargins(0, 0, 0, 0);

            buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
            buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
            buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(GULDEN_DIALOG_CONFIRM_BUTTON_STYLE);

            buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Cancel"));
            buttonBox->button(QDialogButtonBox::Reset)->setCursor(Qt::PointingHandCursor);
            buttonBox->button(QDialogButtonBox::Reset)->setStyleSheet(GULDEN_DIALOG_CANCEL_BUTTON_STYLE);

            d->setLayout(vbox);

            int result = d->exec();
            if (result == QDialog::Accepted) {
                ui->addressBookTabTable->model()->setData(indexes.at(0).sibling(indexes.at(0).row(), 0), lineEditAddress->text(), Qt::EditRole);
                ui->addressBookTabTable->model()->setData(indexes.at(0).sibling(indexes.at(0).row(), 1), lineEditLabel->text(), Qt::EditRole);
            }
        }
    }
}

void GuldenSendCoinsEntry::updateDisplayUnit()
{
    /*if(model && model->getOptionsModel())
    {

        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }*/
}

void GuldenSendCoinsEntry::searchChangedAddressBook(const QString& searchString)
{
    proxyModelRecipients->setFilterFixedString(searchString);

    ui->addressBookTabTable->selectionModel()->clear();
    ui->addressBookTabTable->selectionModel()->setCurrentIndex(proxyModelRecipients->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void GuldenSendCoinsEntry::searchChangedMyAccounts(const QString& searchString)
{
    proxyModelAddresses->setFilterFixedString(searchString);

    ui->myAccountsTabTable->selectionModel()->clear();
    ui->myAccountsTabTable->selectionModel()->setCurrentIndex(proxyModelAddresses->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

bool GuldenSendCoinsEntry::updateLabel(const QString& address)
{
    if (!model)
        return false;

    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if (!associatedLabel.isEmpty()) {

        return true;
    }

    return false;
}
