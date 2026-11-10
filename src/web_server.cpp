#include "web_server.h"

#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

#include "store.h"

namespace {

// HTML embebido. Los emojis van como entidades para no depender de /utf-8 en el compilador.
const char *kPage = R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Canciones OBS</title>
<style>
* { box-sizing: border-box; }
html, body { margin: 0; height: 100%; overflow: hidden; }
body {
  background: #1e1e1e; color: #fff;
  font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
  display: flex; flex-direction: column; height: 100%;
}
#main-content {
  flex: 1; display: flex; flex-direction: column; gap: 12px; padding: 12px;
  overflow-y: auto;
}
button {
  background: #333; color: #fff; border: 0; border-radius: 14px;
  touch-action: manipulation;
  -webkit-tap-highlight-color: transparent;
  transition: background .15s;
}
button.on  { background: #2a4365; }
button.err { background: #7a1e1e; }

/* Vista de Botones */
#view-buttons { display: none; flex-direction: column; gap: 12px; height: 100%; }
#view-buttons.active { display: flex; }
.nav { display: flex; gap: 12px; }
.nav button { flex: 1; height: 22vh; min-height: 110px; font-size: 3.2rem; }
.grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; flex: 1; }
.grid button { font-size: 2.2rem; min-height: 64px; }
.grid .zero { grid-column: 1 / -1; font-size: 1.5rem; }

/* Vista de Lista */
#view-list { display: none; flex-direction: column; gap: 8px; }
#view-list.active { display: flex; }
#btn-refresh-list {
  background: #2a4365; padding: 12px; font-weight: bold; border-radius: 8px; margin-bottom: 8px;
}
details {
  background: #2a2a2a; border-radius: 12px; overflow: hidden;
}
summary {
  padding: 16px; font-size: 1.2rem; font-weight: bold; cursor: pointer;
  list-style: none; display: flex; justify-content: space-between;
}
summary::-webkit-details-marker { display: none; }
summary::after { content: '+'; font-size: 1.5rem; color: #888; }
details[open] summary::after { content: '-'; }
.stanza-btn {
  display: block; width: 100%; text-align: left; background: #222;
  border-radius: 0; padding: 12px 16px; border-top: 1px solid #333;
  font-size: 1rem; white-space: pre-wrap; line-height: 1.4;
}
.stanza-btn:active { background: #2a4365; }

/* Boton de ocultar en la lista */
#btn-hide-list {
  background: #333;
  color: #fff;
  padding: 16px;
  border-radius: 12px;
  font-size: 1.2rem;
  font-weight: bold;
  width: 100%;
  margin-bottom: 8px;
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 8px;
  /* Esto lo hace flotar */
  position: sticky;
  top: 0;
  z-index: 10;
  /* Un borde y sombra para separarlo de la lista al flotar */
  border: 1px solid #444;
  box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5);
}
#btn-hide-list:active {
  background: #2a4365;
}

/* Barra inferior de navegacion */
.bottom-nav {
  display: flex; background: #111; border-top: 1px solid #333;
}
.bottom-nav button {
  flex: 1; padding: 16px; border-radius: 0; font-size: 1rem; font-weight: bold;
  background: transparent; color: #888;
}
.bottom-nav button.active { color: #fff; border-bottom: 3px solid #2a4365; }

#st { text-align: center; font-size: .85rem; color: #ff6b6b; padding: 8px; }
</style>
</head>
<body>
<div id="main-content">
  <!-- VISTA 1: BOTONES -->
<div id="view-buttons">
    <div class="nav">
      <button data-url="/prev">&#11013;&#65039;</button>
      <button data-url="/next">&#10145;&#65039;</button>
    </div>
    <div class="grid">
      <button data-url="/play/1">1</button>
      <button data-url="/play/2">2</button>
      <button data-url="/play/3">3</button>
      <button data-url="/play/4">4</button>
      <button data-url="/play/5">5</button>
      <button data-url="/play/6">6</button>
      <button data-url="/play/7">7</button>
      <button data-url="/play/8">8</button>
      <button data-url="/play/9">9</button>
      <button class="zero" data-url="/toggle_hidden">Ocultar</button>
    </div>
  </div>

  <!-- VISTA 2: LISTA -->
<div id="view-list" class="active">
    <button id="btn-hide-list" data-url="/toggle_hidden" style="background-color: #333;">
      Ocultar
    </button>
    <button id="btn-refresh-list" onclick="loadPlaylist()">
      &#128260; Actualizar lista
    </button>
    <div id="song-list-container"></div>
  </div>
  
  <div id="st"></div>
</div>

<!-- NAVEGACION INFERIOR -->
<div class="bottom-nav">
  <button id="btn-view-list" class="active" onclick="switchView('list')">Lista</button>
  <button id="btn-view-buttons" onclick="switchView('buttons')">Botonera</button>
</div>

<script>
var st = document.getElementById('st');
var playlistLoaded = false;

function send(btn, url) {
  btn.classList.add('on');
  fetch(url, { cache: 'no-store' })
    .then(function (r) { if (!r.ok) throw new Error(); st.textContent = ''; })
    .catch(function () {
      btn.classList.remove('on');
      btn.classList.add('err');
      st.textContent = 'Sin conexion con OBS';
    });
  setTimeout(function () { btn.classList.remove('on', 'err'); }, 180);
}

document.querySelectorAll('button[data-url]').forEach(function (b) {
  b.addEventListener('click', function () { send(b, b.dataset.url); });
});

function switchView(view) {
  document.getElementById('view-buttons').classList.remove('active');
  document.getElementById('view-list').classList.remove('active');
  document.getElementById('btn-view-buttons').classList.remove('active');
  document.getElementById('btn-view-list').classList.remove('active');

  if (view === 'buttons') {
    document.getElementById('view-buttons').classList.add('active');
    document.getElementById('btn-view-buttons').classList.add('active');
  } else {
    document.getElementById('view-list').classList.add('active');
    document.getElementById('btn-view-list').classList.add('active');
    if (!playlistLoaded) {
      loadPlaylist();
    }
  }
}

function loadPlaylist() {
  var container = document.getElementById('song-list-container');
  container.innerHTML = '<div style="text-align:center; padding:20px; color:#888;">Cargando lista...</div>';

  fetch('/api/playlist', { cache: 'no-store' })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      container.innerHTML = '';
      if (!data.playlist || data.playlist.length === 0) {
        container.innerHTML = '<div style="text-align:center; padding:20px; color:#888;">La lista de reproducci&oacute;n est&aacute; vac&iacute;a.</div>';
        return;
      }

      data.playlist.forEach(function(song) {
        var details = document.createElement('details');
        var summary = document.createElement('summary');
        summary.textContent = song.title + (song.artist ? ' - ' + song.artist : '');
        details.appendChild(summary);

        song.stanzas.forEach(function(stanza, index) {
          var btn = document.createElement('button');
          btn.className = 'stanza-btn';
          btn.textContent = stanza.text;
          btn.onclick = function() {
           fetch('/play/song/' + song.id + '/' + index, { cache: 'no-store' })
  .then(function(r) { if (!r.ok) throw new Error(); st.textContent = ''; btn.style.background = '#2a4365'; setTimeout(function(){ btn.style.background = ''; }, 200); })
  .catch(function() { st.textContent = 'Error al conectar'; });
          };
          details.appendChild(btn);
        });

        container.appendChild(details);
      });
      playlistLoaded = true;
    })
    .catch(function() {
      container.innerHTML = '<div style="text-align:center; padding:20px; color:#ff6b6b;">Error al cargar la lista.</div>';
    });
}

// Al cargar la página, la lista está activa por defecto.
loadPlaylist();
</script>
</body>
</html>
)HTML";

} // namespace

WebServer::WebServer(QObject *parent) : QObject(parent), server_(new QTcpServer(this))
{
	connect(server_, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
}

bool WebServer::startServer(quint16 port)
{
	if (server_->isListening())
		return true;
	port_ = port;
	// Any: acepta conexiones desde otros dispositivos de la red (el celular).
	return server_->listen(QHostAddress::Any, port_);
}

void WebServer::stopServer()
{
	server_->close();
}

bool WebServer::isListening() const
{
	return server_->isListening();
}

QString WebServer::errorString() const
{
	return server_->errorString();
}

QString WebServer::getLocalIP() const
{
	QString fallback;
	const auto ifaces = QNetworkInterface::allInterfaces();
	for (const QNetworkInterface &ni : ifaces) {
		const auto f = ni.flags();
		if (!(f & QNetworkInterface::IsUp) || !(f & QNetworkInterface::IsRunning) ||
		    (f & QNetworkInterface::IsLoopBack))
			continue;

		// Adaptadores virtuales (VMware, VirtualBox, Hyper-V, WSL): solo como ultimo recurso.
		const QString name = ni.humanReadableName();
		const bool virt = name.contains("vEthernet", Qt::CaseInsensitive) ||
				  name.contains("VMware", Qt::CaseInsensitive) ||
				  name.contains("VirtualBox", Qt::CaseInsensitive) ||
				  name.contains("Hyper-V", Qt::CaseInsensitive) ||
				  name.contains("WSL", Qt::CaseInsensitive);

		for (const QNetworkAddressEntry &e : ni.addressEntries()) {
			const QHostAddress a = e.ip();
			if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback())
				continue;
			if (a.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16))
				continue;
			if (!virt)
				return a.toString();
			if (fallback.isEmpty())
				fallback = a.toString();
		}
	}
	return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
}

void WebServer::onNewConnection()
{
	while (QTcpSocket *socket = server_->nextPendingConnection())
		handleSocket(socket);
}

void WebServer::handleSocket(QTcpSocket *socket)
{
	auto buf = std::make_shared<QByteArray>();

	connect(socket, &QTcpSocket::readyRead, this, [this, socket, buf]() {
		buf->append(socket->readAll());

		if (buf->size() > 8192) {
			QObject::disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
			sendResponse(socket, 413, "Payload Too Large", "text/plain", "too large");
			return;
		}

		const int end = buf->indexOf("\r\n\r\n");
		if (end < 0)
			return; // todavia no llego el encabezado completo

		QObject::disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
		handleRequest(socket, buf->left(end));
	});

	connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

	// Evita conexiones colgadas.
	QTimer::singleShot(5000, socket, [socket]() {
		if (socket->state() != QAbstractSocket::UnconnectedState)
			socket->abort();
	});
}

void WebServer::handleRequest(QTcpSocket *socket, const QByteArray &head)
{
	const QByteArray firstLine = head.left(head.indexOf("\r\n"));
	const QList<QByteArray> parts = firstLine.split(' ');
	if (parts.size() < 2) {
		sendResponse(socket, 400, "Bad Request", "text/plain", "bad request");
		return;
	}

	const QByteArray method = parts.at(0);
	QByteArray path = parts.at(1);
	const int q = path.indexOf('?');
	if (q >= 0)
		path = path.left(q);

	if (method != "GET") {
		sendResponse(socket, 405, "Method Not Allowed", "text/plain", "method not allowed");
		return;
	}

	if (path == "/") {
		sendResponse(socket, 200, "OK", "text/html; charset=utf-8", QByteArray(kPage));
	} else if (path == "/next") {
		emit nextRequested();
		sendResponse(socket, 200, "OK", "text/plain", "ok");
	} else if (path == "/prev") {
		emit prevRequested();
		sendResponse(socket, 200, "OK", "text/plain", "ok");
	} else if (path == "/toggle_hidden") {
		emit toggleHiddenRequested();
		sendResponse(socket, 200, "OK", "text/plain", "ok");
	} else if (path == "/api/playlist") {
		Store &store = Store::instance();
		QJsonArray arr;
		for (const QString &id : store.playlist) {
			Song *s = store.findSong(id);
			if (!s) continue;
			QJsonObject so;
			so.insert(QStringLiteral("id"), s->id);
			so.insert(QStringLiteral("title"), s->title);
			so.insert(QStringLiteral("artist"), s->artist);
			QJsonArray stArr;
			for (const Stanza &st : s->stanzas) {
				QJsonObject sto;
				sto.insert(QStringLiteral("text"), st.text);
				stArr.append(sto);
			}
			so.insert(QStringLiteral("stanzas"), stArr);
			arr.append(so);
		}
		QJsonObject root;
		root.insert(QStringLiteral("playlist"), arr);
		QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
		sendResponse(socket, 200, "OK", "application/json", json);
	} else if (path.startsWith("/play/song/")) {
		QList<QByteArray> segs = path.split('/');
		if (segs.size() == 5) {
			QString songId = QString::fromUtf8(segs[3]);
			bool ok = false;
			int idx = segs[4].toInt(&ok);
			if (ok && idx >= 0) {
				emit songStanzaRequested(songId, idx);
				sendResponse(socket, 200, "OK", "text/plain", "ok");
				return;
			}
		}
		sendResponse(socket, 400, "Bad Request", "text/plain", "bad song request");
	} else if (path.startsWith("/play/")) {
		bool ok = false;
		const int idx = path.mid(6).toInt(&ok);
		if (ok && idx >= 1 && idx <= 9) {
			emit stanzaRequested(idx);
			sendResponse(socket, 200, "OK", "text/plain", "ok");
		} else {
			sendResponse(socket, 400, "Bad Request", "text/plain", "bad index");
		}
	} else {
		// Incluye /favicon.ico, que el navegador del celular pide solo.
		sendResponse(socket, 404, "Not Found", "text/plain", "not found");
	}
}

void WebServer::sendResponse(QTcpSocket *socket, int code, const QByteArray &reason,
			     const QByteArray &contentType, const QByteArray &body)
{
	QByteArray out;
	out.append("HTTP/1.1 ").append(QByteArray::number(code)).append(' ').append(reason).append("\r\n");
	out.append("Content-Type: ").append(contentType).append("\r\n");
	out.append("Content-Length: ").append(QByteArray::number(body.size())).append("\r\n");
	out.append("Cache-Control: no-store\r\n");
	out.append("Connection: close\r\n\r\n");
	out.append(body);

	socket->write(out);
	socket->disconnectFromHost(); // cierra despues de enviar todo lo pendiente
}
