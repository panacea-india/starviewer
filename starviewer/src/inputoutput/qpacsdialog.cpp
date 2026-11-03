/*************************************************************************************
  Copyright (C) 2014 Laboratori de Gràfics i Imatge, Universitat de Girona &
  Institut de Diagnòstic per la Imatge.
  Girona 2014. All rights reserved.
  http://starviewer.udg.edu

  This file is part of the Starviewer (Medical Imaging Software) open source project.
  It is subject to the license terms in the LICENSE file found in the top-level
  directory of this distribution and at http://starviewer.udg.edu/license. No part of
  the Starviewer (Medical Imaging Software) open source project, including this file,
  may be copied, modified, propagated, or distributed except according to the
  terms contained in the LICENSE file.
 *************************************************************************************/

#include "qpacsdialog.h"

#include "echotopacs.h"
#include "echotowadopacs.h"
#include "logging.h"
#include "pacsconnection.h"
#include "pacscredentialstore.h"
#include "pacsdevicemanager.h"
#include "starviewerapplication.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSslCertificate>
#include <QRegularExpressionValidator>

namespace udg {

QPacsDialog::QPacsDialog(PacsDevice::Type type, QWidget *parent)
    : QDialog(parent), m_pacsType(type), m_cGetChosen(false)
{
    initialize();

    switch (m_pacsType)
    {
        case PacsDevice::Type::Dimse:
            setWindowTitle(tr("New DIMSE PACS server"));
            setupDimse();
            break;

        case PacsDevice::Type::Wado:
            setWindowTitle(tr("New WADO PACS server"));
            setupWado();
            break;

        case PacsDevice::Type::WadoUriDimse:
            setWindowTitle(tr("New WADO-URI + DIMSE PACS server"));
            setupWadoUriDimse();
            break;
    }

    adjustSize();
}

QPacsDialog::QPacsDialog(QString pacsId, QWidget *parent)
    : QDialog(parent), m_pacsId(std::move(pacsId)), m_cGetChosen(false)
{
    initialize();
    setWindowTitle(tr("Edit PACS server"));
    reset();    // this will fill the dialog with the appropriate values
    adjustSize();
}

void QPacsDialog::initialize()
{
    setupUi(this);

    m_aeTitleLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(R"([ !"#$%&'()*+,\-./0-9:;<=>?@A-Z[\]^_`{|}~]{1,16})",
                                                                                       QRegularExpression::CaseInsensitiveOption),
                                                                    m_aeTitleLineEdit));
    createConnections();
}

void QPacsDialog::setupDimse()
{
    m_aeTitleLabel->show();
    m_aeTitleLineEdit->show();
    m_addressLabel->show();
    m_addressLineEdit->show();
    m_qrServiceLabel->show();
    m_qrServiceEnabledCheckBox->show();
    m_qrServicePortLabel->show();
    m_qrServicePortSpinBox->show();
    m_sendServiceLabel->show();
    m_sendServiceEnabledCheckBox->show();
    m_sendServicePortLabel->show();
    m_sendServicePortSpinBox->show();
    m_baseUriLabel->hide();
    m_baseUriLineEdit->hide();
    setLoginFieldsVisible(false);
    setTransportFieldsVisible(true, true);
}

void QPacsDialog::setupWado()
{
    m_aeTitleLabel->hide();
    m_aeTitleLineEdit->hide();
    m_addressLabel->hide();
    m_addressLineEdit->hide();
    m_qrServiceLabel->hide();
    m_qrServiceEnabledCheckBox->hide();
    m_qrServicePortLabel->hide();
    m_qrServicePortSpinBox->hide();
    m_sendServiceLabel->hide();
    m_sendServiceEnabledCheckBox->hide();
    m_sendServicePortLabel->hide();
    m_sendServicePortSpinBox->hide();
    m_baseUriLabel->show();
    m_baseUriLineEdit->show();
    setLoginFieldsVisible(true);
    setTransportFieldsVisible(false, false);
}

void QPacsDialog::setupWadoUriDimse()
{
    m_aeTitleLabel->show();
    m_aeTitleLineEdit->show();
    m_addressLabel->show();
    m_addressLineEdit->show();
    m_qrServiceLabel->show();
    m_qrServiceEnabledCheckBox->show();
    m_qrServiceEnabledCheckBox->setChecked(true);   // force Q/R service enabled in WADO-URI to allow queries
    m_qrServiceEnabledCheckBox->setEnabled(false);
    m_qrServicePortLabel->show();
    m_qrServicePortSpinBox->show();
    m_sendServiceLabel->show();
    m_sendServiceEnabledCheckBox->show();
    m_sendServicePortLabel->show();
    m_sendServicePortSpinBox->show();
    m_baseUriLabel->show();
    m_baseUriLineEdit->show();
    setLoginFieldsVisible(true);
    setTransportFieldsVisible(true, false);
}

void QPacsDialog::setTransportFieldsVisible(bool dicomTls, bool cGet)
{
    m_dicomTlsCheckBox->setVisible(dicomTls);
    m_clientCertificateLabel->setVisible(dicomTls);
    m_clientCertificateLineEdit->setVisible(dicomTls);
    m_clientCertificateBrowseButton->setVisible(dicomTls);
    m_clientPrivateKeyLabel->setVisible(dicomTls);
    m_clientPrivateKeyLineEdit->setVisible(dicomTls);
    m_clientPrivateKeyBrowseButton->setVisible(dicomTls);
    m_cGetCheckBox->setVisible(cGet);

    if (dicomTls && !PACSConnection::isDicomTlsAvailable())
    {
        m_dicomTlsCheckBox->setChecked(false);
        m_dicomTlsCheckBox->setEnabled(false);
        m_dicomTlsCheckBox->setToolTip(tr("This build of %1 cannot use DICOM TLS.").arg(ApplicationNameString));
    }

    updateTransportFields();
}

void QPacsDialog::updateTransportFields()
{
    bool dicomTls = m_pacsType != PacsDevice::Type::Wado && m_dicomTlsCheckBox->isChecked();

    // A WADO-URI + DIMSE PACS needs it for HTTPS even without DICOM TLS.
    bool caCertificate = m_pacsType != PacsDevice::Type::Dimse || dicomTls;
    m_caCertificateLabel->setEnabled(caCertificate);
    m_caCertificateLineEdit->setEnabled(caCertificate);
    m_caCertificateBrowseButton->setEnabled(caCertificate);

    m_clientCertificateLabel->setEnabled(dicomTls);
    m_clientCertificateLineEdit->setEnabled(dicomTls);
    m_clientCertificateBrowseButton->setEnabled(dicomTls);
    m_clientPrivateKeyLabel->setEnabled(dicomTls);
    m_clientPrivateKeyLineEdit->setEnabled(dicomTls);
    m_clientPrivateKeyBrowseButton->setEnabled(dicomTls);

    m_cGetCheckBox->setChecked(dicomTls || m_cGetChosen);
    m_cGetCheckBox->setEnabled(!dicomTls);
}

void QPacsDialog::chooseFile(QLineEdit *lineEdit)
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose a file"), QFileInfo(lineEdit->text()).absolutePath(),
                                                    tr("PEM files (*.pem *.crt *.cer *.key);;All files (*)"));

    if (!fileName.isEmpty())
    {
        lineEdit->setText(QDir::toNativeSeparators(fileName));
    }
}

bool QPacsDialog::areCertificatesValid()
{
    bool dicomTls = m_pacsType != PacsDevice::Type::Wado && m_dicomTlsCheckBox->isChecked();
    QString caCertificateFile = m_caCertificateLineEdit->text().trimmed();

    if (dicomTls && caCertificateFile.isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("DICOM TLS needs a CA certificate file."), QMessageBox::Ok);
        return false;
    }

    if (m_caCertificateLineEdit->isEnabled() && !caCertificateFile.isEmpty()
        && QSslCertificate::fromPath(caCertificateFile, QSsl::Pem, QSslCertificate::PatternSyntax::FixedString).isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("No certificates could be read from the CA certificate file."), QMessageBox::Ok);
        return false;
    }

    if (!dicomTls)
    {
        return true;
    }

    QString certificateFile = m_clientCertificateLineEdit->text().trimmed();
    QString privateKeyFile = m_clientPrivateKeyLineEdit->text().trimmed();

    if (certificateFile.isEmpty() != privateKeyFile.isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("A client certificate and its private key must be given together."), QMessageBox::Ok);
        return false;
    }

    if (!certificateFile.isEmpty() && (!QFileInfo::exists(certificateFile) || !QFileInfo::exists(privateKeyFile)))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The client certificate or its private key file does not exist."), QMessageBox::Ok);
        return false;
    }

    return true;
}

void QPacsDialog::setLoginFieldsVisible(bool visible)
{
    m_usernameLabel->setVisible(visible);
    m_usernameLineEdit->setVisible(visible);
    m_passwordLabel->setVisible(visible);
    m_passwordLineEdit->setVisible(visible);
    updateLoginWarning();
}

void QPacsDialog::createConnections()
{
    connect(m_qrServiceEnabledCheckBox, &QCheckBox::toggled, m_qrServicePortSpinBox, &QWidget::setEnabled);
    connect(m_qrServiceEnabledCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (m_pacsType == PacsDevice::Type::Dimse)
        {
            if (checked) {
                m_defaultQueryPacsCheckBox->setEnabled(true);
            }
            else
            {
                m_defaultQueryPacsCheckBox->setEnabled(false);
                m_defaultQueryPacsCheckBox->setChecked(false);
            }
        }
    });
    connect(m_sendServiceEnabledCheckBox, &QCheckBox::toggled, m_sendServicePortSpinBox, &QWidget::setEnabled);

    connect(m_baseUriLineEdit, &QLineEdit::editingFinished, this, &QPacsDialog::moveLoginFromBaseUriToFields);
    connect(m_baseUriLineEdit, &QLineEdit::textChanged, this, &QPacsDialog::updateLoginWarning);
    connect(m_usernameLineEdit, &QLineEdit::textChanged, this, &QPacsDialog::updateLoginWarning);
    connect(m_baseUriLineEdit, &QLineEdit::textChanged, this, &QPacsDialog::updatePasswordPlaceholder);
    connect(m_usernameLineEdit, &QLineEdit::textChanged, this, &QPacsDialog::updatePasswordPlaceholder);

    connect(m_dicomTlsCheckBox, &QCheckBox::toggled, this, &QPacsDialog::updateTransportFields);
    connect(m_cGetCheckBox, &QCheckBox::clicked, [this](bool checked) { m_cGetChosen = checked; });
    connect(m_caCertificateBrowseButton, &QPushButton::clicked, [this]() { chooseFile(m_caCertificateLineEdit); });
    connect(m_clientCertificateBrowseButton, &QPushButton::clicked, [this]() { chooseFile(m_clientCertificateLineEdit); });
    connect(m_clientPrivateKeyBrowseButton, &QPushButton::clicked, [this]() { chooseFile(m_clientPrivateKeyLineEdit); });

    connect(m_testPushButton, &QPushButton::clicked, this, &QPacsDialog::test);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &QPacsDialog::save);
    connect(m_buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &QPacsDialog::reset);
}

void QPacsDialog::moveLoginFromBaseUriToFields()
{
    QUrl baseUri(m_baseUriLineEdit->text());

    if (!baseUri.userInfo().isEmpty())
    {
        m_usernameLineEdit->setText(baseUri.userName(QUrl::FullyDecoded));
        m_passwordLineEdit->setText(baseUri.password(QUrl::FullyDecoded));
        baseUri.setUserInfo(QString());
        m_baseUriLineEdit->setText(baseUri.toString());
    }
}

bool QPacsDialog::isLoginValid()
{
    if (m_usernameLineEdit->text().contains(':'))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("User name cannot contain a colon."), QMessageBox::Ok);
        return false;
    }

    if (m_usernameLineEdit->text().isEmpty() && !m_passwordLineEdit->text().isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("A password needs a user name."), QMessageBox::Ok);
        return false;
    }

    if (!m_usernameLineEdit->text().isEmpty() && m_passwordLineEdit->text().isEmpty() && !keepsStoredPassword())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("A user name needs a password."), QMessageBox::Ok);
        return false;
    }

    return true;
}

bool QPacsDialog::keepsStoredPassword() const
{
    return m_passwordLineEdit->text().isEmpty() && storedPasswordApplies();
}

bool QPacsDialog::storedPasswordApplies() const
{
    return !m_storedUsername.isEmpty()
        && m_usernameLineEdit->text() == m_storedUsername
        && PacsCredentialStore::getKey(QUrl(m_baseUriLineEdit->text())) == PacsCredentialStore::getKey(m_storedLoginBaseUri);
}

void QPacsDialog::loadStoredLogin()
{
    m_storedLoginBaseUri = QUrl(m_baseUriLineEdit->text());
    m_storedUsername = PacsCredentialStore::load(m_storedLoginBaseUri).username;
    m_usernameLineEdit->setText(m_storedUsername);
    m_passwordLineEdit->clear();
    updatePasswordPlaceholder();
}

void QPacsDialog::updatePasswordPlaceholder()
{
    m_passwordLineEdit->setPlaceholderText(storedPasswordApplies() ? tr("Saved. Leave blank to keep it.") : QString());
}

void QPacsDialog::updateLoginWarning()
{
    m_loginWarningLabel->setVisible(m_pacsType != PacsDevice::Type::Dimse
                                    && !m_usernameLineEdit->text().isEmpty()
                                    && QUrl(m_baseUriLineEdit->text()).scheme() == "http");
}

bool QPacsDialog::arePacsSettingsValid()
{
    moveLoginFromBaseUriToFields();

    if (!areCertificatesValid())
    {
        return false;
    }

    if (m_pacsType == PacsDevice::Type::Dimse)
    {
        if (!m_aeTitleLineEdit->hasAcceptableInput() || m_aeTitleLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("AE Title is empty or has an invalid value."), QMessageBox::Ok);
            return false;
        }

        if (m_addressLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Address cannot be empty."), QMessageBox::Ok);
            return false;
        }

        if (!m_qrServiceEnabledCheckBox->isChecked() && !m_sendServiceEnabledCheckBox->isChecked())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("At least one of the services Query/Retrieve or Send must be enabled."), QMessageBox::Ok);
            return false;
        }
    }
    else if (m_pacsType == PacsDevice::Type::Wado)
    {
        if (m_baseUriLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Base URI cannot be empty."), QMessageBox::Ok);
            return false;
        }

        if (!QUrl(m_baseUriLineEdit->text()).isValid())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Base URI is not a valid URI."), QMessageBox::Ok);
            return false;
        }

        if (!isLoginValid())
        {
            return false;
        }
    }
    else if (m_pacsType == PacsDevice::Type::WadoUriDimse)
    {
        if (!m_aeTitleLineEdit->hasAcceptableInput() || m_aeTitleLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("AE Title is empty or has an invalid value."), QMessageBox::Ok);
            return false;
        }

        if (m_addressLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Address cannot be empty."), QMessageBox::Ok);
            return false;
        }

        if (m_baseUriLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Base URI cannot be empty."), QMessageBox::Ok);
            return false;
        }

        if (!QUrl(m_baseUriLineEdit->text()).isValid())
        {
            QMessageBox::warning(this, ApplicationNameString, tr("Base URI is not a valid URI."), QMessageBox::Ok);
            return false;
        }

        if (!isLoginValid())
        {
            return false;
        }
    }

    return true;
}

PacsDevice QPacsDialog::getPacsDevice() const
{
    PacsDevice pacsDevice;
    pacsDevice.setID(m_pacsId);
    pacsDevice.setType(m_pacsType);
    pacsDevice.setAETitle(m_aeTitleLineEdit->text());
    pacsDevice.setAddress(m_addressLineEdit->text());
    pacsDevice.setQueryRetrieveServiceEnabled(m_qrServiceEnabledCheckBox->isChecked());
    pacsDevice.setQueryRetrieveServicePort(m_qrServicePortSpinBox->value());
    pacsDevice.setStoreServiceEnabled(m_sendServiceEnabledCheckBox->isChecked());
    pacsDevice.setStoreServicePort(m_sendServicePortSpinBox->value());
    pacsDevice.setBaseUri(m_baseUriLineEdit->text());

    if (m_pacsType != PacsDevice::Type::Dimse && !keepsStoredPassword())
    {
        pacsDevice.setLogin(m_usernameLineEdit->text(), m_passwordLineEdit->text());
    }

    bool dicomTls = m_pacsType != PacsDevice::Type::Wado && m_dicomTlsCheckBox->isChecked();
    pacsDevice.setCaCertificateFile(m_pacsType != PacsDevice::Type::Dimse || dicomTls ? m_caCertificateLineEdit->text().trimmed() : QString());
    pacsDevice.setDicomTlsEnabled(dicomTls);

    if (dicomTls)
    {
        pacsDevice.setClientCertificate(m_clientCertificateLineEdit->text().trimmed(), m_clientPrivateKeyLineEdit->text().trimmed());
    }

    pacsDevice.setCGetEnabled(m_pacsType == PacsDevice::Type::Dimse && m_cGetChosen);

    pacsDevice.setInstitution(m_institutionLineEdit->text());
    pacsDevice.setLocation(m_locationLineEdit->text());
    pacsDevice.setDescription(m_descriptionLineEdit->text());
    pacsDevice.setDefault(m_defaultQueryPacsCheckBox->isChecked());

    return pacsDevice;
}

void QPacsDialog::test()
{
    if (!arePacsSettingsValid())
    {
        return;
    }

    // Agafem les dades del PACS que estan el textbox per testejar
    PacsDevice pacsDevice = getPacsDevice();

    if (pacsDevice.getType() == PacsDevice::Type::Dimse || pacsDevice.getType() == PacsDevice::Type::WadoUriDimse)
    {
        EchoToPACS echoToPACS;

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        INFO_LOG(QString("Doing C-ECHO to PACS with AE Title %1.").arg(pacsDevice.getAETitle()));
        bool success = echoToPACS.echo(pacsDevice);
        QApplication::restoreOverrideCursor();

        if (success)
        {
            QMessageBox::information(this, ApplicationNameString, tr("Test of PACS \"%1\" is correct").arg(pacsDevice.getAETitle()), QMessageBox::Ok);
        }
        else
        {
            QString message;

            switch (echoToPACS.getLastError())
            {
                case EchoToPACS::EchoFailed:
                    message = tr("PACS \"%1\" did not respond correctly.\nMake sure its address and AE Title are correct.").arg(pacsDevice.getAETitle());
                    break;
                case EchoToPACS::EchoCanNotConnectToPACS:
                    message = tr("PACS \"%1\" did not respond.\nMake sure its address and AE Title are correct.").arg(pacsDevice.getAETitle());

                    if (pacsDevice.isDicomTlsEnabled())
                    {
                        message += "\n" + tr("With DICOM TLS, also make sure that its certificate is for this address and that the certificate files are correct.");
                    }

                    break;
                default:
                    // Should never happen
                    message = tr("PACS \"%1\" did not respond as expected, an unknown error has occurred.").arg(pacsDevice.getAETitle());
                    break;
            }

            QMessageBox::information(this, ApplicationNameString, message, QMessageBox::Ok);
        }
    }
    else if (pacsDevice.getType() == PacsDevice::Type::Wado)
    {
        EchoToWadoPacs echoToWadoPacs;

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        INFO_LOG(QString("Doing ECHO to PACS with base URI %1.").arg(pacsDevice.getBaseUri().toString()));
        bool success = echoToWadoPacs.echo(pacsDevice);
        QApplication::restoreOverrideCursor();

        if (success)
        {
            QMessageBox::information(this, ApplicationNameString, tr("Test is correct."));
        }
        else
        {
            QMessageBox::warning(this, ApplicationNameString, echoToWadoPacs.getErrors());
        }
    }
    else
    {
        QMessageBox::warning(this, ApplicationNameString, tr("Unknown PACS type."));
    }
}

void QPacsDialog::accept()
{
    if (save())
    {
        QDialog::accept();
    }
}

bool QPacsDialog::save()
{
    if (!arePacsSettingsValid())
    {
        return false;
    }

    PacsDevice pacsDevice = getPacsDevice();

    if (m_pacsId.isEmpty())
    {
        if (m_pacsType == PacsDevice::Type::Dimse)
        {
            INFO_LOG(QString("Adding new DIMSE PACS with AE Title %1.").arg(pacsDevice.getAETitle()));
        }
        else if (m_pacsType == PacsDevice::Type::Wado)
        {
            INFO_LOG(QString("Adding new WADO PACS with base URI %1.").arg(pacsDevice.getBaseUri().toString()));
        }
        else if (m_pacsType == PacsDevice::Type::WadoUriDimse)
        {
            INFO_LOG(QString("Adding new WADO-URI + DIMSE PACS with base URI %1 and AE Title %2.").arg(pacsDevice.getBaseUri().toString())
                                                                                                  .arg(pacsDevice.getAETitle()));
        }

        if (PacsDeviceManager::addPacs(pacsDevice))
        {
            m_pacsId = pacsDevice.getID();
            finishSavingLogin(pacsDevice);
            return true;
        }
        else
        {
            QMessageBox::warning(this, ApplicationNameString, tr("This PACS already exists."), QMessageBox::Ok);
            return false;
        }
    }
    else
    {
        INFO_LOG(QString("Updating PACS with ID %1.").arg(pacsDevice.getID()));
        PacsDeviceManager::updatePacs(pacsDevice);
        finishSavingLogin(pacsDevice);
        return true;
    }
}

void QPacsDialog::finishSavingLogin(const PacsDevice &pacsDevice)
{
    if (pacsDevice.getType() == PacsDevice::Type::Dimse)
    {
        return;
    }

    if (PacsCredentialStore::isKeptOnlyForThisRun(pacsDevice.getBaseUri()))
    {
        QMessageBox::warning(this, ApplicationNameString, tr("The login could not be saved securely on this computer. It will be kept only until you close %1.")
                                                          .arg(ApplicationNameString), QMessageBox::Ok);
    }

    loadStoredLogin();
}

void QPacsDialog::reset()
{
    if (m_pacsId.isEmpty())
    {
        m_aeTitleLineEdit->clear();
        m_addressLineEdit->clear();
        m_qrServiceEnabledCheckBox->setChecked(true);
        m_qrServicePortSpinBox->setValue(11112);
        m_sendServiceEnabledCheckBox->setChecked(true);
        m_sendServicePortSpinBox->setValue(11112);
        m_baseUriLineEdit->clear();
        m_usernameLineEdit->clear();
        m_passwordLineEdit->clear();
        m_passwordLineEdit->setPlaceholderText(QString());
        m_storedUsername.clear();
        m_storedLoginBaseUri.clear();
        m_cGetChosen = false;
        m_dicomTlsCheckBox->setChecked(false);
        m_caCertificateLineEdit->clear();
        m_clientCertificateLineEdit->clear();
        m_clientPrivateKeyLineEdit->clear();
        m_institutionLineEdit->clear();
        m_locationLineEdit->clear();
        m_descriptionLineEdit->clear();
        m_defaultQueryPacsCheckBox->setChecked(false);
    }
    else
    {
        PacsDevice pacsDevice = PacsDeviceManager::getPacsDeviceById(m_pacsId);
        m_pacsType = pacsDevice.getType();
        m_aeTitleLineEdit->setText(pacsDevice.getAETitle());
        m_addressLineEdit->setText(pacsDevice.getAddress());
        m_qrServiceEnabledCheckBox->setChecked(pacsDevice.isQueryRetrieveServiceEnabled());
        m_qrServicePortSpinBox->setValue(pacsDevice.getQueryRetrieveServicePort());
        m_sendServiceEnabledCheckBox->setChecked(pacsDevice.isStoreServiceEnabled());
        m_sendServicePortSpinBox->setValue(pacsDevice.getStoreServicePort());
        m_baseUriLineEdit->setText(pacsDevice.getBaseUri().toString());
        loadStoredLogin();
        m_cGetChosen = pacsDevice.isCGetEnabled();
        m_dicomTlsCheckBox->setChecked(pacsDevice.isDicomTlsEnabled());
        m_caCertificateLineEdit->setText(pacsDevice.getCaCertificateFile());
        m_clientCertificateLineEdit->setText(pacsDevice.getClientCertificateFile());
        m_clientPrivateKeyLineEdit->setText(pacsDevice.getClientPrivateKeyFile());
        m_institutionLineEdit->setText(pacsDevice.getInstitution());
        m_locationLineEdit->setText(pacsDevice.getLocation());
        m_descriptionLineEdit->setText(pacsDevice.getDescription());
        m_defaultQueryPacsCheckBox->setChecked(pacsDevice.isDefault());

        if (m_pacsType == PacsDevice::Type::Dimse)
        {
            setupDimse();
        }
        else if (m_pacsType == PacsDevice::Type::Wado)
        {
            setupWado();
        }
        else if (m_pacsType == PacsDevice::Type::WadoUriDimse)
        {
            setupWadoUriDimse();
        }
    }

    updateTransportFields();
}

} // namespace udg
