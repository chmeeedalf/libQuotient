// SPDX-FileCopyrightText: 2018 Kitsune Ral <kitsune-ral@users.sf.net>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "util.h"

#include <QtNetwork/QNetworkAccessManager>

namespace Quotient {

class QUOTIENT_API NetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    explicit NetworkAccessManager(QObject* parent = nullptr);

    void addBaseUrl(const QString& accountId, const QUrl& homeserver);
    void dropBaseUrl(const QString& accountId);
    QList<QSslError> ignoredSslErrors() const;
    void addIgnoredSslError(const QSslError& error);
    void clearIgnoredSslErrors();
    void ignoreSslErrors(bool ignore = true) const;

    //! Get a NAM instance for the current thread
    static NetworkAccessManager* instance();

private Q_SLOTS:
    QStringList supportedSchemesImplementation() const; // clazy:exclude=const-signal-or-slot

private:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                                 QIODevice* outgoingData = Q_NULLPTR) override;

    class Private;
    ImplPtr<Private> d;
};
} // namespace Quotient
