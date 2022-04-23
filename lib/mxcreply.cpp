// SPDX-FileCopyrightText: Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mxcreply.h"

#include <QtCore/QBuffer>
#include "accountregistry.h"
#include "room.h"

#ifdef Quotient_E2EE_ENABLED
#include "events/encryptedfile.h"
#endif

using namespace Quotient;

class MxcReply::Private
{
public:
    explicit Private(QNetworkReply* r = nullptr)
        : m_reply(r)
    {}
    QNetworkReply* m_reply;
    Omittable<EncryptedFile> m_encryptedFile;
    QIODevice* m_device = nullptr;
};

MxcReply::MxcReply(QNetworkReply* reply)
    : d(makeImpl<Private>(reply))
{
    d->m_device = d->m_reply;
    reply->setParent(this);
    connect(d->m_reply, &QNetworkReply::finished, this, [this]() {
        setError(d->m_reply->error(), d->m_reply->errorString());
        setOpenMode(ReadOnly);
        Q_EMIT finished();
    });
}

MxcReply::MxcReply(QNetworkReply* reply, Room* room, const QString &eventId)
    : d(makeImpl<Private>(reply))
{
    reply->setParent(this);
    connect(d->m_reply, &QNetworkReply::finished, this, [this]() {
        setError(d->m_reply->error(), d->m_reply->errorString());

#ifdef Quotient_E2EE_ENABLED
        if(!d->m_encryptedFile.has_value()) {
            d->m_device = d->m_reply;
        } else {
            EncryptedFile file = *d->m_encryptedFile;
            auto buffer = new QBuffer(this);
            buffer->setData(file.decryptFile(d->m_reply->readAll()));
            buffer->open(ReadOnly);
            d->m_device = buffer;
        }
#else
        d->m_device = d->m_reply;
#endif
        setOpenMode(ReadOnly);
        emit finished();
    });

#ifdef Quotient_E2EE_ENABLED
    auto eventIt = room->findInTimeline(eventId);
    if(eventIt != room->historyEdge()) {
        auto event = eventIt->viewAs<RoomMessageEvent>();
        d->m_encryptedFile = event->content()->fileInfo()->file;
    }
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define ERROR_SIGNAL errorOccurred
#else
#define ERROR_SIGNAL error
#endif

MxcReply::MxcReply()
    : d(ZeroImpl<Private>())
{
    static const auto BadRequestPhrase = tr("Bad Request");
    QMetaObject::invokeMethod(this, [this]() {
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 400);
            setAttribute(QNetworkRequest::HttpReasonPhraseAttribute,
                         BadRequestPhrase);
            setError(QNetworkReply::ProtocolInvalidOperationError,
                     BadRequestPhrase);
            setFinished(true);
            emit ERROR_SIGNAL(QNetworkReply::ProtocolInvalidOperationError);
            emit finished();
        }, Qt::QueuedConnection);
}

qint64 MxcReply::readData(char *data, qint64 maxSize)
{
    return d->m_device->read(data, maxSize);
}

void MxcReply::abort()
{
    d->m_reply->abort();
}
