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

#include "pacsdevicemanager.h"

#include "coresettings.h"
#include "inputoutputsettings.h"
#include "logging.h"
#include "pacscredentialstore.h"
#include "pacsdevice.h"

namespace udg {

namespace {

// Returns true if the given PACS is already configured and false otherwise.
bool isPacsConfigured(const PacsDevice &pacs)
{
    QList<PacsDevice> pacsList = PacsDeviceManager::getPacsList();

    foreach (PacsDevice pacsDevice, pacsList)
    {
        if (pacsDevice.isSamePacsDevice(pacs))
        {
            return true;
        }
    }

    return false;
}

// Returns true if a configured PACS has the given base URI and false otherwise.
bool isBaseUriConfigured(const QUrl &baseUri)
{
    for (const PacsDevice &pacs : PacsDeviceManager::getPacsList())
    {
        if (!pacs.getBaseUri().isEmpty() && PacsCredentialStore::getKey(pacs.getBaseUri()) == PacsCredentialStore::getKey(baseUri))
        {
            return true;
        }
    }

    return false;
}

// Removes the stored login for the given base URI unless a configured PACS still uses it.
void removeLoginIfUnused(const QUrl &baseUri)
{
    if (!isBaseUriConfigured(baseUri))
    {
        PacsCredentialStore::remove(baseUri);
    }
}

// Converts a PacsDevice instance to a Settings::SettingsListItemType instance.
Settings::SettingsListItemType pacsDeviceToSettingsListItem(const PacsDevice &pacsDevice)
{
    Settings::SettingsListItemType item;
    item["ID"] = pacsDevice.getID();

    switch (pacsDevice.getType())
    {
        case PacsDevice::Type::Dimse:
            item["Type"] = "DIMSE";
            item["AETitle"] = pacsDevice.getAETitle();
            item["PacsHostname"] = pacsDevice.getAddress();
            item["QueryRetrieveServiceEnabled"] = pacsDevice.isQueryRetrieveServiceEnabled();
            item["PacsPort"] = pacsDevice.getQueryRetrieveServicePort();
            item["StoreServiceEnabled"] = pacsDevice.isStoreServiceEnabled();
            item["StoreServicePort"] = pacsDevice.getStoreServicePort();
            break;

        case PacsDevice::Type::Wado:
            item["Type"] = "WADO";
            item["BaseUri"] = pacsDevice.getBaseUri();
            break;

        case PacsDevice::Type::WadoUriDimse:
            item["Type"] = "WADO-URI+DIMSE";
            item["AETitle"] = pacsDevice.getAETitle();
            item["PacsHostname"] = pacsDevice.getAddress();
            item["QueryRetrieveServiceEnabled"] = pacsDevice.isQueryRetrieveServiceEnabled();
            item["PacsPort"] = pacsDevice.getQueryRetrieveServicePort();
            item["StoreServiceEnabled"] = pacsDevice.isStoreServiceEnabled();
            item["StoreServicePort"] = pacsDevice.getStoreServicePort();
            item["BaseUri"] = pacsDevice.getBaseUri();
    }

    // These are only written when used, so that entries that don't use them stay as they were.
    if (!pacsDevice.getCaCertificateFile().isEmpty())
    {
        item["CaCertificateFile"] = pacsDevice.getCaCertificateFile();
    }

    if (pacsDevice.isOAuthEnabled() && pacsDevice.getType() != PacsDevice::Type::Dimse)
    {
        item["OAuthTokenUri"] = pacsDevice.getOAuthTokenUri();
        item["OAuthClientId"] = pacsDevice.getOAuthClientId();
    }

    if (pacsDevice.getType() != PacsDevice::Type::Wado && pacsDevice.isDicomTlsEnabled())
    {
        item["DicomTlsEnabled"] = true;

        if (!pacsDevice.getClientCertificateFile().isEmpty())
        {
            item["ClientCertificateFile"] = pacsDevice.getClientCertificateFile();
            item["ClientPrivateKeyFile"] = pacsDevice.getClientPrivateKeyFile();
        }
    }

    if (pacsDevice.getType() == PacsDevice::Type::Dimse && pacsDevice.isCGetEnabled())
    {
        item["CGetEnabled"] = true;
    }

    item["Institution"] = pacsDevice.getInstitution();
    item["Location"] = pacsDevice.getLocation();
    item["Description"] = pacsDevice.getDescription();

    return item;
}

// Converts a Settings::SettingsListItemType instance to a PacsDevice instance.
PacsDevice settingsListItemToPacsDevice(const Settings::SettingsListItemType &item)
{
    PacsDevice pacsDevice;
    pacsDevice.setID(item["ID"].toString());

    if (!item.contains("Type") || item["Type"].toString() == "DIMSE")
    {
        pacsDevice.setType(PacsDevice::Type::Dimse);
        pacsDevice.setAETitle(item["AETitle"].toString());
        pacsDevice.setAddress(item["PacsHostname"].toString());
        pacsDevice.setQueryRetrieveServiceEnabled(item.value("QueryRetrieveServiceEnabled", true).toBool());
        pacsDevice.setQueryRetrieveServicePort(item["PacsPort"].toInt());

        if (!item.contains("StoreServiceEnabled"))
        {
            pacsDevice.setStoreServiceEnabled(true);
            pacsDevice.setStoreServicePort(item["PacsPort"].toInt());
        }
        else
        {
            pacsDevice.setStoreServiceEnabled(item["StoreServiceEnabled"].toBool());
            pacsDevice.setStoreServicePort(item["StoreServicePort"].toInt());
        }
    }
    else if (item["Type"].toString() == "WADO")
    {
        pacsDevice.setType(PacsDevice::Type::Wado);
        pacsDevice.setBaseUri(item["BaseUri"].toUrl());
    }
    else if (item["Type"].toString() == "WADO-URI+DIMSE")
    {
        pacsDevice.setType(PacsDevice::Type::WadoUriDimse);
        pacsDevice.setAETitle(item["AETitle"].toString());
        pacsDevice.setAddress(item["PacsHostname"].toString());
        pacsDevice.setQueryRetrieveServiceEnabled(item.value("QueryRetrieveServiceEnabled", true).toBool());
        pacsDevice.setQueryRetrieveServicePort(item["PacsPort"].toInt());

        if (!item.contains("StoreServiceEnabled"))
        {
            pacsDevice.setStoreServiceEnabled(true);
            pacsDevice.setStoreServicePort(item["PacsPort"].toInt());
        }
        else
        {
            pacsDevice.setStoreServiceEnabled(item["StoreServiceEnabled"].toBool());
            pacsDevice.setStoreServicePort(item["StoreServicePort"].toInt());
        }

        pacsDevice.setBaseUri(item["BaseUri"].toUrl());
    }

    pacsDevice.setCaCertificateFile(item.value("CaCertificateFile").toString());
    // The client secret is not here: it is kept in PacsCredentialStore under the token URI
    pacsDevice.setOAuth(item.value("OAuthTokenUri").toUrl(), item.value("OAuthClientId").toString(), QString());
    pacsDevice.setDicomTlsEnabled(item.value("DicomTlsEnabled", false).toBool());
    pacsDevice.setClientCertificate(item.value("ClientCertificateFile").toString(), item.value("ClientPrivateKeyFile").toString());
    pacsDevice.setCGetEnabled(item.value("CGetEnabled", false).toBool());

    pacsDevice.setInstitution(item["Institution"].toString());
    pacsDevice.setLocation(item["Location"].toString());
    pacsDevice.setDescription(item["Description"].toString());

    return pacsDevice;
}

}

bool PacsDeviceManager::addPacs(PacsDevice &pacs)
{
    if (isPacsConfigured(pacs))
    {
        return false;   // PACS already exists
    }

    QList<PacsDevice> pacsList = getPacsList();

    // Assign ID. It will be 0 if there is no other PACS and the highest ID + 1 otherwise.
    int newId = pacsList.size();    // initialize directly to the size; it will probably be the final id

    for (const PacsDevice &pacs : pacsList)
    {
        int id = pacs.getID().toInt();

        if (id >= newId)
        {
            newId = id + 1;
        }
    }

    pacs.setID(QString::number(newId));

    Settings().addListItem(InputOutputSettings::PacsListConfigurationSectionName, pacsDeviceToSettingsListItem(pacs));

    if (pacs.isOAuthEnabled() && !pacs.getOAuthClientSecret().isEmpty())
    {
        PacsCredentialStore::save(pacs.getOAuthTokenUri(), PacsLogin{pacs.getOAuthClientId(), pacs.getOAuthClientSecret()});
    }

    if (pacs.isLoginSpecified())
    {
        if (pacs.getUsername().isEmpty())
        {
            PacsCredentialStore::remove(pacs.getBaseUri());
        }
        else
        {
            PacsCredentialStore::save(pacs.getBaseUri(), PacsLogin{pacs.getUsername(), pacs.getPassword()});
        }
    }

    return true;
}

void PacsDeviceManager::updatePacs(PacsDevice &pacsToUpdate)
{
    PacsDevice oldPacs = getPacsDeviceById(pacsToUpdate.getID());

    if (oldPacs.isEmpty())
    {
        return; // PACS does not exist
    }

    QList<PacsDevice> pacsList = getPacsList();

    // Delete all PACS and then add them again with one of them updated
    Settings().remove(InputOutputSettings::PacsListConfigurationSectionName);

    for (PacsDevice &device : pacsList)
    {
        if (pacsToUpdate.getID() == device.getID())
        {
            addPacs(pacsToUpdate);
        }
        else
        {
            addPacs(device);
        }
    }

    if (PacsCredentialStore::getKey(oldPacs.getBaseUri()) != PacsCredentialStore::getKey(pacsToUpdate.getBaseUri()))
    {
        removeLoginIfUnused(oldPacs.getBaseUri());
    }
}

void PacsDeviceManager::moveLoginsToCredentialStore()
{
    Settings settings;
    Settings::SettingListType pacsList = settings.getList(InputOutputSettings::PacsListConfigurationSectionName);
    QString defaultPacsList = settings.getValue(CoreSettings::DefaultPACSListToQuery2).toString();
    QString oldDefaultPacsList = settings.getValue(CoreSettings::DefaultPACSListToQuery).toString();
    bool changed = false;

    for (Settings::SettingsListItemType &item : pacsList)
    {
        QUrl baseUri = item.value("BaseUri").toUrl();

        if (baseUri.userInfo().isEmpty())
        {
            continue;
        }

        QUrl baseUriWithoutLogin = baseUri.adjusted(QUrl::RemoveUserInfo);

        if (PacsCredentialStore::save(baseUriWithoutLogin, PacsLogin{baseUri.userName(QUrl::FullyDecoded), baseUri.password(QUrl::FullyDecoded)}))
        {
            item["BaseUri"] = baseUriWithoutLogin;
            defaultPacsList.replace(baseUri.toString(), baseUriWithoutLogin.toString());
            oldDefaultPacsList.replace(baseUri.toString(), baseUriWithoutLogin.toString());
            changed = true;
            INFO_LOG(QString("Moved the login of PACS %1 to the secure store.").arg(baseUriWithoutLogin.toString()));
        }
    }

    if (changed)
    {
        settings.setList(InputOutputSettings::PacsListConfigurationSectionName, pacsList);
        settings.setValue(CoreSettings::DefaultPACSListToQuery2, defaultPacsList);
        settings.setValue(CoreSettings::DefaultPACSListToQuery, oldDefaultPacsList);
    }
}

void PacsDeviceManager::deletePacs(const QString &pacsID)
{
    PacsDevice pacsToDelete = getPacsDeviceById(pacsID);

    if (pacsToDelete.isEmpty())
    {
        return; // PACS does not exist
    }

    QList<PacsDevice> pacsList = getPacsList();

    // Delete all PACS and then add them again except for the deleted one
    Settings().remove(InputOutputSettings::PacsListConfigurationSectionName);

    for (PacsDevice &device : pacsList)
    {
        if (pacsID != device.getID())
        {
            addPacs(device);
        }
    }

    removeLoginIfUnused(pacsToDelete.getBaseUri());
}

QList<PacsDevice> PacsDeviceManager::getPacsList(PacsFilter filter)
{
    Settings::SettingListType list = Settings().getList(InputOutputSettings::PacsListConfigurationSectionName);
    QList<PacsDevice> configuredPacsList;

    for (const Settings::SettingsListItemType &item : list)
    {
        PacsDevice pacs = settingsListItemToPacsDevice(item);
        bool include = filter.testFlag(CanRetrieve) && (
                    ((pacs.getType() == PacsDevice::Type::Dimse || pacs.getType() == PacsDevice::Type::WadoUriDimse) && pacs.isQueryRetrieveServiceEnabled())
                    || pacs.getType() == PacsDevice::Type::Wado
                    );
        include |= filter.testFlag(CanStore) && (
                    ((pacs.getType() == PacsDevice::Type::Dimse || pacs.getType() == PacsDevice::Type::WadoUriDimse) && pacs.isStoreServiceEnabled())
                    || pacs.getType() == PacsDevice::Type::Wado
                    );
        include &= filter.testFlag(OnlyDefault) ? pacs.isDefault() : true;

        if (include)
        {
            configuredPacsList.append(pacs);
        }
    }

    return configuredPacsList;
}

PacsDevice PacsDeviceManager::getPacsDeviceById(const QString &pacsID)
{
    QList<PacsDevice> pacsList = getPacsList();

    for (const PacsDevice &pacs : pacsList)
    {
        if (pacs.getID() == pacsID)
        {
            return pacs;
        }
    }

    WARN_LOG(QString("PACS with ID %1 not found.").arg(pacsID));

    return PacsDevice();
}

QList<PacsDevice> PacsDeviceManager::removeDuplicatePacsFromList(const QList<PacsDevice> &pacsDeviceList)
{
    QList<PacsDevice> pacsDeviceListWithoutDuplicates;

    for (const PacsDevice &pacsDevice : pacsDeviceList)
    {
        if (!isAddedSamePacsDeviceInList(pacsDeviceListWithoutDuplicates, pacsDevice))
        {
            pacsDeviceListWithoutDuplicates.append(pacsDevice);
        }
    }

    return pacsDeviceListWithoutDuplicates;
}

bool PacsDeviceManager::isAddedSamePacsDeviceInList(const QList<PacsDevice> &pacsDeviceList, const PacsDevice &pacsDevice)
{
    for (const PacsDevice &pacsDeviceInList : pacsDeviceList)
    {
        if (pacsDeviceInList.isSamePacsDevice(pacsDevice))
        {
            return true;
        }
    }

    return false;
}

}
