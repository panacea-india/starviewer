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

#ifndef UDG_QPACSDIALOG_H
#define UDG_QPACSDIALOG_H

#include <QDialog>
#include "ui_qpacsdialogbase.h"

#include "pacsdevice.h"

namespace udg {

/**
 * @brief The QPacsDialog class allows the user to edit the settings of a PACS server.
 */
class QPacsDialog : public QDialog, private ::Ui::QPacsDialogBase
{
    Q_OBJECT
public:
    /// Creates the dialog with empty fields, to add a new PACS of the given type.
    explicit QPacsDialog(PacsDevice::Type type, QWidget *parent = nullptr);
    /// Creates the dialog and fills it with the information of the PACS with the given id.
    explicit QPacsDialog(QString pacsId, QWidget *parent = nullptr);

private:
    /// Common initialization for both constructors.
    void initialize();

    /// Leaves the dialog ready to configure a DIMSE PACS.
    void setupDimse();

    /// Leaves the dialog ready to configure a WADO PACS.
    void setupWado();

    /// Leaves the dialog ready to configure a WADO-URI + DIMSE PACS.
    void setupWadoUriDimse();

    /// Creates the needed connections.
    void createConnections();

    /// Shows or hides the login fields.
    void setLoginFieldsVisible(bool visible);

    /// Shows or hides the OAuth 2 fields.
    void setOAuthFieldsVisible(bool visible);

    /// Returns true if OAuth 2 is chosen and applies to this type of PACS.
    bool isOAuthChosen() const;

    /// Returns true if the current values in the OAuth 2 fields are valid and false otherwise, in which case it shows a warning.
    bool isOAuthValid();

    /// Returns true if the stored client secret must be kept: the token URI and the client ID are the stored ones and no secret has been entered.
    bool keepsStoredOAuthSecret() const;

    /// Returns true if the stored client secret applies to the current token URI and client ID.
    bool storedOAuthSecretApplies() const;

    /// Fills the OAuth 2 fields from the given PACS. The stored client secret is never put in the dialog.
    void loadOAuth(const PacsDevice &pacsDevice);

    /// Shows or hides the fields for DICOM TLS and C-GET, and shows the CA certificate field, which all types have.
    void setTransportFieldsVisible(bool dicomTls, bool cGet);

    /// Returns true if the current values in the certificate fields are valid and false otherwise, in which case it shows a warning.
    bool areCertificatesValid();

    /// Asks for a file and puts it in the given field.
    void chooseFile(QLineEdit *lineEdit);

    /// Returns true if the current values in the login fields are valid and false otherwise, in which case it shows a warning.
    bool isLoginValid();

    /// Returns true if the current values mean that the stored password must be kept: the base URI and the user name are the stored ones and no password has been
    /// entered. A changed base URI needs the password to be entered again, so that the stored one is not sent to another server.
    bool keepsStoredPassword() const;

    /// Returns true if the stored password applies to the current base URI and user name.
    bool storedPasswordApplies() const;

    /// Reads the stored user name for the current base URI into the login fields. The stored password is never put in the dialog.
    void loadStoredLogin();

    /// Returns true if the current values in all fields are valid and false otherwise.
    bool arePacsSettingsValid();

    /// Creates and returns a PacsDevice with the current values in the dialog.
    PacsDevice getPacsDevice() const;

    /// To be called after saving the given PACS. Warns if its login could not be kept across runs and leaves the login fields as they are now stored.
    void finishSavingLogin(const PacsDevice &pacsDevice);

private slots:
    /// Enables the fields for certificates and C-GET according to whether DICOM TLS is on.
    void updateTransportFields();

    /// Shows the warning about sending a password without encryption if it applies to the current values.
    void updateLoginWarning();

    /// Enables the OAuth 2 fields while it is chosen, and the login fields while it is not.
    void updateOAuthFields();

    /// Says in the client secret field that the stored secret will be kept, only while it applies to the current token URI and client ID.
    void updateOAuthSecretPlaceholder();

    /// If the token URI field has a login, moves it to the client ID and secret fields, so that it's never saved or shown in the token URI.
    void moveOAuthLoginFromTokenUriToFields();

    /// Says in the password field that the stored password will be kept, only while it applies to the current base URI and user name.
    void updatePasswordPlaceholder();

    /// If the base URI field has a login, moves it to the login fields, so that it's never saved or shown in the base URI.
    void moveLoginFromBaseUriToFields();

    /// Tests the PACS defined by the current values in the dialog with a C-ECHO.
    void test();

    /// Calls save() and if that returns true closes the dialog.
    void accept() override;

    /// If the current values are valid, saves the PACS (adding or updating it). Otherwise, shows a warning. Returns true if the PACS was correctly saved.
    bool save();

    /// Resets the values of all fields to their initial values: empty for a new PACS and the values from settings when editing one.
    void reset();

private:
    /// Type for a new PACS.
    PacsDevice::Type m_pacsType;
    /// Id of the edited PACS, if any.
    QString m_pacsId;
    /// Base URI whose stored login is the one read into the dialog.
    QUrl m_storedLoginBaseUri;
    /// Stored user name read into the dialog, if any.
    QString m_storedUsername;
    /// Token URI and client ID of the PACS read into the dialog, if it used OAuth 2 and has a client secret stored.
    QUrl m_storedOAuthTokenUri;
    QString m_storedOAuthClientId;
    /// Whether C-GET is chosen, apart from DICOM TLS, which always uses it. The C-GET box shows this while DICOM TLS is off.
    bool m_cGetChosen;
};

} // namespace udg

#endif // UDG_QPACSDIALOG_H
