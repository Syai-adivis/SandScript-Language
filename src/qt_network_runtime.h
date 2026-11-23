#ifndef QT_NETWORK_RUNTIME_H
#define QT_NETWORK_RUNTIME_H

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <stdexcept>
#include <string>

namespace qt_network_runtime
{
inline void ensureApplication()
{
#ifdef SAND_ENABLE_QT_WIDGETS
    qt_widget_runtime_detail::ensureApplication();
#else
    if (QCoreApplication::instance())
        return;

    static int argc = 1;
    static char applicationName[] = "SandScript";
    static char *argv[] = {applicationName, nullptr};
    static std::unique_ptr<QCoreApplication> ownedApplication =
        std::make_unique<QCoreApplication>(argc, argv);
#endif
}

inline std::string request(const std::string &method, const std::string &url,
                           const std::string &body = std::string())
{
    ensureApplication();

    const QUrl requestUrl(QString::fromStdString(url), QUrl::StrictMode);
    if (!requestUrl.isValid() || requestUrl.host().isEmpty() ||
        (requestUrl.scheme() != QStringLiteral("http") &&
         requestUrl.scheme() != QStringLiteral("https")))
    {
        throw std::runtime_error("HTTP request URL must be a valid http or https URL");
    }

    QNetworkAccessManager manager;
    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = nullptr;
    if (method == "GET")
        reply = manager.get(networkRequest);
    else
    {
        networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                 QStringLiteral("text/plain; charset=utf-8"));
        reply = manager.post(networkRequest, QByteArray::fromStdString(body));
    }

    QEventLoop eventLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, [&]() {
        timedOut = true;
        reply->abort();
    });
    timeout.start(30000);
    if (!reply->isFinished())
        eventLoop.exec();
    timeout.stop();

    const QByteArray response = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (timedOut)
        throw std::runtime_error("HTTP request timed out after 30000 ms");
    if (reply->error() != QNetworkReply::NoError)
    {
        const std::string status = statusCode == 0
                                       ? std::string()
                                       : " (HTTP " + std::to_string(statusCode) + ")";
        throw std::runtime_error("HTTP request failed" + status + ": " +
                                 reply->errorString().toStdString());
    }

    return QString::fromUtf8(response).toStdString();
}
}

#endif
