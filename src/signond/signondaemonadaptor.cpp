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

#include "signondaemonadaptor.h"

#include "signonauthsessionadaptor.h"
#include "signondisposable.h"
#include "signonidentityadaptor.h"
#include "accesscontrolmanagerhelper.h"

namespace SignonDaemonNS {

SignonDaemonAdaptor::SignonDaemonAdaptor(SignonDaemon *parent):
    QDBusAbstractAdaptor(parent),
    m_parent(parent)
{
    setAutoRelaySignals(false);
}

SignonDaemonAdaptor::~SignonDaemonAdaptor()
{
}

template <class T>
QDBusObjectPath
SignonDaemonAdaptor::registerObject(const QDBusConnection &connection,
                                    T *object)
{
    QString path = object->objectName();

    QObject *registeredObject = connection.objectRegisteredAt(path);
    if (!registeredObject || registeredObject->parent() != object) {
        QDBusConnection conn(connection);
        auto adaptor = new typename T::Adaptor(object);
        if (!conn.registerObject(path, adaptor,
                                 QDBusConnection::ExportAllContents)) {
            BLAME() << "Object registration failed:" << object <<
                conn.lastError();
        }
    }
    return QDBusObjectPath(path);
}

QDBusObjectPath SignonDaemonAdaptor::registerNewIdentity(const QString &applicationContext)
{
    Q_UNUSED(applicationContext);

    SignonIdentity *identity = m_parent->registerNewIdentity();

    QDBusConnection dbusConnection(parentDBusContext().connection());
    QDBusObjectPath objectPath = registerObject(dbusConnection, identity);

    SignonDisposable::destroyUnused();
    return objectPath;
}

void SignonDaemonAdaptor::securityErrorReply()
{
    securityErrorReply(parentDBusContext().connection(),
                       parentDBusContext().message());
}

void SignonDaemonAdaptor::securityErrorReply(const QDBusConnection &conn,
                                             const QDBusMessage &msg)
{
    QString errMsg;
    QTextStream(&errMsg) << SIGNOND_PERMISSION_DENIED_ERR_STR
                         << "Method:"
                         << msg.member();

    msg.setDelayedReply(true);
    QDBusMessage errReply =
                msg.createErrorReply(SIGNOND_PERMISSION_DENIED_ERR_NAME,
                                     errMsg);
    conn.send(errReply);
    TRACE() << "Method FAILED Access Control check:" << msg.member();
}

bool SignonDaemonAdaptor::handleLastError(const QDBusConnection &conn,
                                          const QDBusMessage &msg)
{
    if (!m_parent->lastErrorIsValid()) return false;

    msg.setDelayedReply(true);
    QDBusMessage errReply =
                msg.createErrorReply(m_parent->lastErrorName(),
                                     m_parent->lastErrorMessage());
    conn.send(errReply);
    return true;
}

void SignonDaemonAdaptor::getIdentity(const quint32 id,
                                      const QString &applicationContext,
                                      QDBusObjectPath &objectPath,
                                      QVariantMap &identityData)
{
    Q_UNUSED(applicationContext);

    AccessControlManagerHelper *acm = AccessControlManagerHelper::instance();
    QDBusMessage msg = parentDBusContext().message();
    QDBusConnection conn = parentDBusContext().connection();
    if (!acm->isPeerAllowedToUseIdentity(PeerContext(conn, msg), id)) {
        SignOn::AccessReply *reply =
            acm->requestAccessToIdentity(PeerContext(conn, msg), id);
        QObject::connect(reply, SIGNAL(finished()),
                         this, SLOT(onIdentityAccessReplyFinished()));
        msg.setDelayedReply(true);
        return;
    }

    SignonIdentity *identity = m_parent->getIdentity(id, identityData);
    if (handleLastError(conn, msg)) return;

    objectPath = registerObject(conn, identity);

    SignonDisposable::destroyUnused();
}

void SignonDaemonAdaptor::onIdentityAccessReplyFinished()
{
    SignOn::AccessReply *reply = qobject_cast<SignOn::AccessReply*>(sender());
    Q_ASSERT(reply != 0);

    reply->deleteLater();
    QDBusConnection connection = reply->request().peerConnection();
    QDBusMessage message = reply->request().peerMessage();
    quint32 id = reply->request().identity();
    AccessControlManagerHelper *acm = AccessControlManagerHelper::instance();

    if (!reply->isAccepted() ||
        !acm->isPeerAllowedToUseIdentity(PeerContext(connection, message), id)) {
        securityErrorReply(connection, message);
        return;
    }

    QVariantMap identityData;
    SignonIdentity *identity = m_parent->getIdentity(id, identityData);
    if (handleLastError(connection, message)) return;

    QDBusObjectPath objectPath = registerObject(connection, identity);

    QVariantList args;
    args << QVariant::fromValue(objectPath);
    args << identityData;
    connection.send(message.createReply(args));

    SignonDisposable::destroyUnused();
}

QStringList SignonDaemonAdaptor::queryMethods()
{
    return m_parent->queryMethods();
}

QDBusObjectPath SignonDaemonAdaptor::getAuthSessionObjectPath(const quint32 id,
                                             const QString &applicationContext,
                                             const QString &type)
{
    Q_UNUSED(applicationContext);

    SignonDisposable::destroyUnused();

    AccessControlManagerHelper *acm = AccessControlManagerHelper::instance();
    QDBusMessage msg = parentDBusContext().message();
    QDBusConnection conn = parentDBusContext().connection();

    /* Access Control */
    if (id != SIGNOND_NEW_IDENTITY) {
        if (!acm->isPeerAllowedToUseIdentity(PeerContext(conn, msg), id)) {
            SignOn::AccessReply *reply =
                acm->requestAccessToIdentity(PeerContext(conn, msg), id);
            /* If the request is accepted, we'll need the method name ("type")
             * in order to proceed with the creation of the authsession. */
            reply->setProperty("type", type);
            QObject::connect(reply, SIGNAL(finished()),
                             this, SLOT(onAuthSessionAccessReplyFinished()));
            msg.setDelayedReply(true);
            return QDBusObjectPath();
        }
    }

    TRACE() << "ACM passed, creating AuthSession object";
    pid_t ownerPid = acm->pidOfPeer(PeerContext(conn, msg));
    SignonAuthSession *authSession = m_parent->getAuthSession(id, type, ownerPid);
    if (handleLastError(conn, msg)) return QDBusObjectPath();

    return registerObject(conn, authSession);
}

void SignonDaemonAdaptor::onAuthSessionAccessReplyFinished()
{
    SignOn::AccessReply *reply = qobject_cast<SignOn::AccessReply*>(sender());
    Q_ASSERT(reply != 0);

    reply->deleteLater();
    QDBusConnection connection = reply->request().peerConnection();
    QDBusMessage message = reply->request().peerMessage();
    quint32 id = reply->request().identity();
    QString type = reply->property("type").toString();
    AccessControlManagerHelper *acm = AccessControlManagerHelper::instance();

    if (!reply->isAccepted() ||
        !acm->isPeerAllowedToUseIdentity(PeerContext(connection, message), id)) {
        securityErrorReply(connection, message);
        TRACE() << "still not allowed";
        return;
    }

    pid_t ownerPid = acm->pidOfPeer(PeerContext(connection, message));
    SignonAuthSession *authSession =
        m_parent->getAuthSession(id, type, ownerPid);
    if (handleLastError(connection, message)) return;
    QDBusObjectPath objectPath = registerObject(connection, authSession);

    QVariantList args;
    args << QVariant::fromValue(objectPath);
    connection.send(message.createReply(args));

    SignonDisposable::destroyUnused();
}

QStringList SignonDaemonAdaptor::queryMechanisms(const QString &method)
{
    QStringList mechanisms = m_parent->queryMechanisms(method);
    if (handleLastError(parentDBusContext().connection(),
                        parentDBusContext().message())) {
        return QStringList();
    }

    return mechanisms;
}

void SignonDaemonAdaptor::queryIdentities(const QVariantMap &filter,
                                          const QString &applicationContext)
{
    Q_UNUSED(applicationContext);

    /* Access Control */
    QDBusMessage msg = parentDBusContext().message();
    QDBusConnection conn = parentDBusContext().connection();
    if (!AccessControlManagerHelper::instance()->isPeerKeychainWidget(
                                                    PeerContext(conn, msg))) {
        securityErrorReply();
        return;
    }

    msg.setDelayedReply(true);
    MapList identities = m_parent->queryIdentities(filter);
    if (handleLastError(conn, msg)) return;

    QDBusMessage reply = msg.createReply(QVariant::fromValue(identities));
    conn.send(reply);
}

bool SignonDaemonAdaptor::clear()
{
    /* Access Control */
    QDBusMessage msg = parentDBusContext().message();
    QDBusConnection conn = parentDBusContext().connection();
    if (!AccessControlManagerHelper::instance()->isPeerKeychainWidget(
                                                    PeerContext(conn, msg))) {
        securityErrorReply();
        return false;
    }

    bool ok = m_parent->clear();
    if (handleLastError(conn, msg)) return false;

    return ok;
}

} //namespace SignonDaemonNS
