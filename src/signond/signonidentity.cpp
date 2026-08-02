/*
 * This file is part of signon
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2013-2016 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "signonidentity.h"

#include "error.h"
#include "signond-common.h"
#include "signonui_interface.h"
#include "SignOn/uisessiondata.h"
#include "SignOn/uisessiondata_priv.h"
#include "signoncommon.h"

#include "accesscontrolmanagerhelper.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>
#include <iostream>

#define SIGNON_RETURN_IF_CAM_NOT_AVAILABLE_ASYNC0() \
    if (!(CredentialsAccessManager::instance()->credentialsSystemOpened())) { \
        Error error = \
            Error(Error::InternalServer, \
                  internalServerErrStr + \
                  QLatin1String("Could not access Signon Database."));\
        callback(error); \
        return; \
    }

#define SIGNON_RETURN_IF_CAM_NOT_AVAILABLE_ASYNC1(ret) \
    if (!(CredentialsAccessManager::instance()->credentialsSystemOpened())) { \
        Error error = \
            Error(Error::InternalServer, \
                  internalServerErrStr + \
                  QLatin1String("Could not access Signon Database."));\
        callback(ret, error); \
        return; \
    }

#define SIGNON_RETURN_IF_CAM_NOT_AVAILABLE() do {                          \
        if (!(CredentialsAccessManager::instance()->credentialsSystemOpened())) { \
            return Error(Error::InternalServer, \
                         internalServerErrStr + \
                         QLatin1String("Could not access Signon Database.")); \
        } \
    } while(0)

namespace SignonDaemonNS {

const QString internalServerErrStr = SIGNOND_INTERNAL_SERVER_ERR_STR;

SignonIdentity::SignonIdentity(quint32 id, int timeout,
                               SignonDaemon *parent):
    SignonDisposable(timeout, parent),
    m_pInfo(NULL),
    m_destroyed(false)
{
    m_id = id;

    /*
     * creation of unique name for the given identity
     * */
    static quint32 incr = 0;
    QString objectName = SIGNOND_DAEMON_OBJECTPATH + QLatin1String("/Identity_")
                         + QString::number(incr++, 16);
    setObjectName(objectName);

    m_signonui = new SignonUiInterface(SIGNON_UI_SERVICE,
                                       SIGNON_UI_DAEMON_OBJECTPATH,
                                       QDBusConnection::sessionBus(),
                                       this);

    /* Watch for credential updates happening outside of this object (this can
     * happen on request of authentication plugins) */
    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    QObject::connect(db, SIGNAL(credentialsUpdated(quint32)),
                     this, SLOT(onCredentialsUpdated(quint32)));
}

SignonIdentity::~SignonIdentity()
{
    if (!m_destroyed) {
        m_destroyed = true;
        Q_EMIT unregistered();
    }

    delete m_signonui;
    delete m_pInfo;
}

SignonIdentity *SignonIdentity::createIdentity(quint32 id, SignonDaemon *parent)
{
    return new SignonIdentity(id, parent->identityTimeout(), parent);
}

void SignonIdentity::destroy()
{
    m_destroyed = true;
    Q_EMIT unregistered();
    deleteLater();
}

SignonIdentityInfo SignonIdentity::queryInfo(bool &ok, bool queryPassword)
{
    ok = true;

    bool needLoadFromDB = true;
    if (m_pInfo) {
        needLoadFromDB = false;
        if (queryPassword && m_pInfo->password().isEmpty()) {
            needLoadFromDB = true;
        }
    }

    if (needLoadFromDB) {
        if (m_pInfo != 0) {
            delete m_pInfo;
        }

        CredentialsDB *db =
            CredentialsAccessManager::instance()->credentialsDB();
        m_pInfo = new SignonIdentityInfo(db->credentials(m_id, queryPassword));

        if (db->lastError().isValid()) {
            ok = false;
            delete m_pInfo;
            m_pInfo = NULL;
            return SignonIdentityInfo();
        }
    }

    /* Make sure that we clear the password, if the caller doesn't need it */
    SignonIdentityInfo info = *m_pInfo;
    if (!queryPassword) {
        info.setPassword(QString());
    }
    return info;
}

Error SignonIdentity::addReference(const QString &reference, const QString &appId)
{
    TRACE() << "addReference: " << reference;

    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE();

    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    if (db == NULL) {
        BLAME() << "NULL database handler object.";
        return Error::InternalServer;
    }
    keepInUse();
    bool ok = db->addReference(m_id, appId, reference);
    return ok ? Error::NoError : Error::OperationFailed;
}

Error SignonIdentity::removeReference(const QString &reference, const QString &appId)
{
    TRACE() << "removeReference: " << reference;

    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE();

    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    if (db == NULL) {
        BLAME() << "NULL database handler object.";
        return Error::InternalServer;
    }
    keepInUse();
    bool ok = db->removeReference(m_id, appId, reference);
    return ok ? Error::NoError : Error::OperationFailed;
}

void SignonIdentity::requestCredentialsUpdate(const QString &displayMessage,
                                              const CredentialsUpdateCb &callback)
{
    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE_ASYNC1(SIGNOND_NEW_IDENTITY);

    bool ok;
    SignonIdentityInfo info = queryInfo(ok, false);

    if (!ok) {
        BLAME() << "Identity not found.";
        callback(SIGNOND_NEW_IDENTITY, Error(Error::IdentityNotFound));
        return;
    }
    if (!info.storePassword()) {
        BLAME() << "Password cannot be stored.";
        callback(SIGNOND_NEW_IDENTITY, Error(Error::StoreFailed));
        return;
    }

    //create ui request to ask password
    QVariantMap uiRequest;
    uiRequest.insert(SSOUI_KEY_QUERYPASSWORD, true);
    uiRequest.insert(SSOUI_KEY_USERNAME, info.userName());
    uiRequest.insert(SSOUI_KEY_MESSAGE, displayMessage);
    uiRequest.insert(SSOUI_KEY_CAPTION, info.caption());

    TRACE() << "Waiting for reply from signon-ui";
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(m_signonui->queryDialog(uiRequest), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, callback]() {
        queryUiSlot(watcher, callback);
    });

    setAutoDestruct(false);
}

Error SignonIdentity::getInfo(SignonIdentityInfo *info)
{
    TRACE() << "QUERYING INFO";

    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE();

    bool ok;
    *info = queryInfo(ok, false);
    info->removeSecrets();

    if (!ok) {
        TRACE();
        return Error(Error::CredentialsNotAvailable,
                     SIGNOND_CREDENTIALS_NOT_AVAILABLE_ERR_STR +
                     QLatin1String("Database querying error occurred."));
    }

    if (info->isNew()) {
        TRACE();
        return Error::IdentityNotFound;
    }

    keepInUse();
    return Error::NoError;
}

void SignonIdentity::queryUserPassword(const QVariantMap &params,
                                       const VerifyUserCb &callback)
{
    TRACE() << "Waiting for reply from signon-ui";
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(m_signonui->queryDialog(params), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, callback]() {
        verifyUiSlot(watcher, callback);
    });

    setAutoDestruct(false);
}

void SignonIdentity::verifyUser(const QVariantMap &params,
                                const VerifyUserCb &callback)
{
    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE_ASYNC1(false);

    bool ok;
    SignonIdentityInfo info = queryInfo(ok, true);

    if (!ok) {
        BLAME() << "Identity not found.";
        callback(false, Error(Error::IdentityNotFound));
        return;
    }
    if (!info.storePassword() || info.password().isEmpty()) {
        BLAME() << "Password is not stored.";
        callback(false, Error(Error::CredentialsNotAvailable));
        return;
    }

    //create ui request to ask password
    QVariantMap uiRequest;
    uiRequest.insert(params);
    uiRequest.insert(SSOUI_KEY_QUERYPASSWORD, true);
    uiRequest.insert(SSOUI_KEY_USERNAME, info.userName());
    uiRequest.insert(SSOUI_KEY_CAPTION, info.caption());

    queryUserPassword(uiRequest, callback);
}

Error SignonIdentity::verifySecret(const QString &secret, bool *verified)
{
    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE();

    bool ok;
    queryInfo(ok);
    if (!ok) {
        TRACE();
        return Error(Error::CredentialsNotAvailable,
                     SIGNOND_CREDENTIALS_NOT_AVAILABLE_ERR_STR +
                     QLatin1String("Database querying error occurred."));
    }

    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    *verified = db->checkPassword(m_pInfo->id(), m_pInfo->userName(), secret);

    keepInUse();
    return Error::NoError;
}

void SignonIdentity::remove(const RemoveCb &callback)
{
    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE_ASYNC0();

    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    if ((db == 0) || !db->removeCredentials(m_id)) {
        TRACE() << "Error occurred while inserting/updating credentials.";
        callback(Error(Error::RemoveFailed,
                       SIGNOND_REMOVE_FAILED_ERR_STR +
                       QLatin1String("Database error occurred.")));
        return;
    }
    setAutoDestruct(false);
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(m_signonui->removeIdentityData(m_id),
                                    this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, callback]() {
        removeCompleted(watcher, callback);
    });
    keepInUse();
}

void SignonIdentity::removeCompleted(QDBusPendingCallWatcher *call,
                                     const RemoveCb &callback)
{
    Q_ASSERT(call != NULL);

    setAutoDestruct(true);
    call->deleteLater();

    QDBusPendingReply<> signOnUiReply = *call;
    bool ok = !signOnUiReply.isError();
    TRACE() << (ok ? "removeIdentityData succeeded" : "removeIdentityData failed");

    emit infoUpdated((int)SignOn::IdentityRemoved);

    callback(Error::none());
}

void SignonIdentity::signOut(const SignOutCb &callback)
{
    TRACE() << "Signout request. Identity ID: " << id();
    /*
     * - If the identity is stored (thus registered here)
     * signal 'sign out' to all identities subsribed to this object,
     * otherwise the only identity subscribed to this is the newly
     * created client side identity, which called this method.
     * - This is just a safety check, as the client identity - if it is a new
     * one - should not inform server side to sign out.
     */
    if (id() != SIGNOND_NEW_IDENTITY) {
        //clear stored sessiondata
        CredentialsDB *db =
            CredentialsAccessManager::instance()->credentialsDB();
        if ((db == 0) || !db->removeData(m_id)) {
            TRACE() << "clear data failed";
        }

        setAutoDestruct(false);
        QDBusPendingCallWatcher *watcher =
            new QDBusPendingCallWatcher(m_signonui->removeIdentityData(m_id),
                                        this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, callback]() {
            signOutCompleted(watcher, callback);
        });
    }
    keepInUse();
}

void SignonIdentity::signOutCompleted(QDBusPendingCallWatcher *call,
                                      const SignOutCb &callback)
{
    Q_ASSERT(call != NULL);

    setAutoDestruct(true);
    call->deleteLater();

    QDBusPendingReply<> signOnUiReply = *call;
    bool ok = !signOnUiReply.isError();
    TRACE() << (ok ? "removeIdentityData succeeded" : "removeIdentityData failed");

    emit infoUpdated((int)SignOn::IdentitySignedOut);

    callback(ok, Error::none());
}

void SignonIdentity::onCredentialsUpdated(quint32 id)
{
    if (id != m_id) return;

    TRACE() << m_id;

    /* Clear the cached information about the identity; some of it might not be
     * valid anymore */
    if (m_pInfo) {
        delete m_pInfo;
        m_pInfo = NULL;
    }

    emit infoUpdated((int)SignOn::IdentityDataUpdated);
}

Error SignonIdentity::store(const QVariantMap &info, const QString &appId,
                            quint32 *id)
{
    keepInUse();
    SIGNON_RETURN_IF_CAM_NOT_AVAILABLE();

    const QVariant container = info.value(SIGNOND_IDENTITY_INFO_AUTHMETHODS);
    MethodMap methods = container.isValid() ?
        qdbus_cast<MethodMap>(container.value<QDBusArgument>()) : MethodMap();

    if (m_pInfo == 0) {
        m_pInfo = new SignonIdentityInfo(info);
        m_pInfo->setMethods(methods);
        //Add creator to owner list if it has AID
        QStringList ownerList =
            info.value(SIGNOND_IDENTITY_INFO_OWNER).toStringList();
        if (!appId.isNull()) {
            ownerList.append(appId);
        }
        m_pInfo->setOwnerList(ownerList);
    } else {
        SignonIdentityInfo newInfo(info);
        m_pInfo->update(newInfo);
    }

    m_id = storeCredentials(*m_pInfo);
    *id = m_id;

    return m_id == SIGNOND_NEW_IDENTITY ? Error::StoreFailed : Error::NoError;
}

quint32 SignonIdentity::storeCredentials(const SignonIdentityInfo &info)
{
    CredentialsDB *db = CredentialsAccessManager::instance()->credentialsDB();
    if (db == NULL) {
        BLAME() << "NULL database handler object.";
        return SIGNOND_NEW_IDENTITY;
    }

    bool newIdentity = info.isNew();

    if (newIdentity)
        m_id = db->insertCredentials(info);
    else
        db->updateCredentials(info);

    if (db->errorOccurred()) {
        if (newIdentity)
            m_id = SIGNOND_NEW_IDENTITY;

        TRACE() << "Error occurred while inserting/updating credentials.";
    } else {
        if (m_pInfo) {
            delete m_pInfo;
            m_pInfo = NULL;
        }
        Q_EMIT stored(this);

        TRACE() << "FRESH, JUST STORED CREDENTIALS ID:" << m_id;
        emit infoUpdated((int)SignOn::IdentityDataUpdated);
    }
    return m_id;
}

void SignonIdentity::queryUiSlot(QDBusPendingCallWatcher *call,
                                 const CredentialsUpdateCb &callback)
{
    TRACE();
    Q_ASSERT(call != NULL);

    setAutoDestruct(true);

    QDBusMessage errReply;
    QDBusPendingReply<QVariantMap> reply = *call;
    call->deleteLater();

    QVariantMap resultParameters;
    if (!reply.isError() && reply.count()) {
        resultParameters = reply.argumentAt<0>();
    } else {
        callback(0, Error(Error::OperationCanceled));
        return;
    }

    if (!resultParameters.contains(SSOUI_KEY_ERROR)) {
        //no reply code
        callback(0, Error(Error::InternalServer));
        return;
    }

    int errorCode = resultParameters.value(SSOUI_KEY_ERROR).toInt();
    TRACE() << "error: " << errorCode;
    if (errorCode != QUERY_ERROR_NONE) {
        Error error;
        if (errorCode == QUERY_ERROR_CANCELED) {
            error = Error(Error::OperationCanceled);
        } else {
            error = Error(Error::InternalServer,
                          QString(QLatin1String("signon-ui call returned error %1")).
                          arg(errorCode));
        }
        callback(0, error);
        return;
    }

    if (resultParameters.contains(SSOUI_KEY_PASSWORD)) {
        CredentialsDB *db =
            CredentialsAccessManager::instance()->credentialsDB();
        if (db == NULL) {
            BLAME() << "NULL database handler object.";
            callback(0, Error(Error::StoreFailed));
            return;
        }

        //store new password
        if (m_pInfo) {
            m_pInfo->setPassword(resultParameters[SSOUI_KEY_PASSWORD].toString());

            quint32 ret = db->updateCredentials(*m_pInfo);
            delete m_pInfo;
            m_pInfo = NULL;
            if (ret != SIGNOND_NEW_IDENTITY) {
                callback(m_id, Error::none());
                return;
            } else{
                BLAME() << "Error during update";
            }
        }
    }

    //this should not happen, return error
    callback(0, Error(Error::InternalServer));
    return;
}

void SignonIdentity::verifyUiSlot(QDBusPendingCallWatcher *call,
                                  const VerifyUserCb &callback)
{
    TRACE();
    Q_ASSERT(call != NULL);

    setAutoDestruct(true);

    QDBusMessage errReply;
    QDBusPendingReply<QVariantMap> reply = *call;
    call->deleteLater();
    QVariantMap resultParameters;
    if (!reply.isError() && reply.count()) {
        resultParameters = reply.argumentAt<0>();
    } else {
        callback(false, Error(Error::OperationCanceled));
        return;
    }

    if (!resultParameters.contains(SSOUI_KEY_ERROR)) {
        //no reply code
        callback(false, Error(Error::InternalServer));
        return;
    }

    int errorCode = resultParameters.value(SSOUI_KEY_ERROR).toInt();
    TRACE() << "error: " << errorCode;
    if (errorCode != QUERY_ERROR_NONE) {
        Error error;
        if (errorCode == QUERY_ERROR_CANCELED) {
            error = Error(Error::OperationCanceled);
        } else if (errorCode == QUERY_ERROR_FORGOT_PASSWORD) {
            error = Error(Error::ForgotPassword);
        } else {
            error = Error(Error::InternalServer,
                          QString(QLatin1String("signon-ui call "
                                                "returned error %1")).
                          arg(errorCode));
        }

        callback(false, error);
        return;
    }

    if (resultParameters.contains(SSOUI_KEY_PASSWORD)) {
        CredentialsDB *db =
            CredentialsAccessManager::instance()->credentialsDB();
        if (db == NULL) {
            BLAME() << "NULL database handler object.";
            callback(false, Error(Error::StoreFailed));
            return;
        }

        //compare passwords
        if (m_pInfo) {
            bool ret =
                m_pInfo->password() == resultParameters[SSOUI_KEY_PASSWORD].
                toString();

            if (!ret && resultParameters.contains(SSOUI_KEY_CONFIRMCOUNT)) {
                int count = resultParameters[SSOUI_KEY_CONFIRMCOUNT].toInt();
                TRACE() << "retry count:" << count;
                if (count > 0) { //retry
                    resultParameters[SSOUI_KEY_CONFIRMCOUNT] = (count-1);
                    resultParameters[SSOUI_KEY_MESSAGEID] =
                        QUERY_MESSAGE_NOT_AUTHORIZED;
                    queryUserPassword(resultParameters, callback);
                    return;
                } else {
                    //TODO show error note here if needed
                }
            }
            delete m_pInfo;
            m_pInfo = NULL;
            callback(ret, Error::none());
            return;
        }
    }
    //this should not happen, return error
    callback(false, Error(Error::InternalServer));
    return;
}

} //namespace SignonDaemonNS
