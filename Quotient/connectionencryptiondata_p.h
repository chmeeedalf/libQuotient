#pragma once

#include "connection.h"
#include "database.h"

#include "e2ee/qolmaccount.h"
#include "e2ee/qolmsession.h"

namespace Quotient {

struct DevicesList;
class EncryptedEvent;

namespace _impl {
    class ConnectionEncryptionData {
    public:
        static Omittable<std::unique_ptr<ConnectionEncryptionData>> setup(
            Connection* connection, bool mock = false);

        Connection* q;
        QOlmAccount olmAccount;
        // No easy way in C++ to discern between SQL SELECT from UPDATE, too bad
        mutable Database database;
        UnorderedMap<QByteArray, std::vector<QOlmSession>> olmSessions;
        //! A map from SenderKey to vector of InboundSession
        QHash<QString, KeyVerificationSession*> verificationSessions{};
        QSet<QString> trackedUsers{};
        QSet<QString> outdatedUsers{};
        QHash<QString, QHash<QString, DeviceKeys>> deviceKeys{};
        QueryKeysJob* currentQueryKeysJob = nullptr;
        QSet<std::pair<QString, QString>> triedDevices{};
        //! An update of internal tracking structures (trackedUsers, e.g.) is
        //! needed
        bool encryptionUpdateRequired = false;
        QHash<QString, int> oneTimeKeysCount{};
        std::vector<std::unique_ptr<EncryptedEvent>> pendingEncryptedEvents{};
        bool isUploadingKeys = false;
        bool firstSync = true;

        void saveDevicesList();
        void loadDevicesList();
        QString curveKeyForUserDevice(const QString& userId,
                                      const QString& device) const;
        bool isKnownCurveKey(const QString& userId,
                             const QString& curveKey) const;
        bool hasOlmSession(const QString &user, const QString &deviceId) const;

        void onSyncSuccess(SyncData &syncResponse);
        void loadOutdatedUserDevices();
        void consumeToDeviceEvents(Events&& toDeviceEvents);
        void encryptionUpdate(const QList<User*>& forUsers);

        bool createOlmSession(const QString& targetUserId,
                              const QString& targetDeviceId,
                              const OneTimeKeys& oneTimeKeyObject);
        void saveSession(const QOlmSession& session, const QByteArray& senderKey)
        {
            database.saveOlmSession(senderKey, session,
                                    QDateTime::currentDateTime());
        }
        void saveOlmAccount()
        {
            qCDebug(E2EE) << "Saving olm account";
            database.storeOlmAccount(olmAccount);
        }

        std::pair<QByteArray, QByteArray> sessionDecryptMessage(
            const QJsonObject& personalCipherObject,
            const QByteArray& senderKey);
        std::pair<EventPtr, QByteArray> sessionDecryptMessage(
            const EncryptedEvent& encryptedEvent);

        QJsonObject assembleEncryptedContent(
            QJsonObject payloadJson, const QString& targetUserId,
            const QString& targetDeviceId) const;
        void sendSessionKeyToDevices(
            const QString& roomId,
            const QOlmOutboundGroupSession& outboundSession,
            const QMultiHash<QString, QString>& devices);

        template <typename... ArgTs>
        KeyVerificationSession* setupKeyVerificationSession(
            ArgTs&&... sessionArgs)
        {
            auto session =
                new KeyVerificationSession(std::forward<ArgTs>(sessionArgs)...);
            verificationSessions.insert(session->transactionId(), session);
            QObject::connect(session, &QObject::destroyed, q,
                             [this, txnId = session->transactionId()] {
                                 verificationSessions.remove(txnId);
                             });
            emit q->newKeyVerificationSession(session);
            return session;
        }

        // This is only public to enable std::make_unique; do not use directly,
        // get an instance from setup() instead
        ConnectionEncryptionData(Connection* connection,
                                 PicklingKey&& picklingKey);

    private:
        void consumeDevicesList(const DevicesList &devicesList);
        bool processIfVerificationEvent(const Event& evt, bool encrypted);
        void handleEncryptedToDeviceEvent(const EncryptedEvent& event);
        void handleQueryKeys(const QueryKeysJob *job);

        // This function assumes that an olm session with (user, device) exists
        std::pair<QOlmMessage::Type, QByteArray> olmEncryptMessage(
            const QString& userId, const QString& device,
            const QByteArray& message) const;

        void doSendSessionKeyToDevices(const QString& roomId, const QByteArray& sessionId,
            const QByteArray &sessionKey, uint32_t messageIndex,
            const QMultiHash<QString, QString>& devices);
    };
} // namespace _impl
} // namespace Quotient
