#pragma once

#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

// Servidor HTTP minimo para controlar las estrofas desde el celular.
// Corre en el hilo principal con eventos de Qt: no bloquea OBS.
class WebServer : public QObject {
	Q_OBJECT

public:
	explicit WebServer(QObject *parent = nullptr);

	bool startServer(quint16 port = 8080);
	void stopServer();
	bool isListening() const;
	quint16 port() const { return port_; }
	QString errorString() const;
	QString getLocalIP() const;

signals:
	void nextRequested();
	void prevRequested();
	void stanzaRequested(int index);
	void toggleHiddenRequested();
	void songStanzaRequested(const QString &songId, int index);

private slots:
	void onNewConnection();

private:
	void handleSocket(QTcpSocket *socket);
	void handleRequest(QTcpSocket *socket, const QByteArray &head);
	void sendResponse(QTcpSocket *socket, int code, const QByteArray &reason,
			  const QByteArray &contentType, const QByteArray &body);

	QTcpServer *server_ = nullptr;
	quint16 port_ = 8080;
};
