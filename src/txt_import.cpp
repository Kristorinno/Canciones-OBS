#include "txt_import.h"

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString normalizeNewlines(const QString &text)
{
	QString t = text;
	t.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	t.replace(QLatin1Char('\r'), QLatin1Char('\n'));
	return t;
}

} // namespace

QStringList splitStanzas(const QString &text)
{
	QStringList result;
	QStringList current;

	const QStringList lines = normalizeNewlines(text).split(QLatin1Char('\n'));
	for (const QString &raw : lines) {
		const QString line = raw.trimmed();
		if (line.isEmpty()) {
			if (!current.isEmpty()) {
				result << current.join(QLatin1Char('\n'));
				current.clear();
			}
		} else {
			current << line;
		}
	}
	if (!current.isEmpty())
		result << current.join(QLatin1Char('\n'));
	return result;
}
Stanza parseStanza(const QString &text)
{
	Stanza st;
	st.text = text;
	st.fontScale = -1; // -1 significa que no hay comando ##

	// Busca el comando ##NUMERO al principio de la estrofa
	static const QRegularExpression sizeRe(QStringLiteral("^##(\\d+)\\s*"));
	QRegularExpressionMatch match = sizeRe.match(st.text);
	if (match.hasMatch()) {
		int value = match.captured(1).toInt();
		if (value == 0) {
			st.fontScale = 100; // ##0 es el tamaño base del tema
		} else {
			st.fontScale = 100 + value; // ##25 pasa a ser 125%, ##5 pasa a ser 105%
		}
		st.text.remove(0, match.capturedLength());
	}
	return st;
}
bool songFromTxtFile(const QString &path, Song &song)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	const QByteArray bytes = file.readAll();

	// Primero se prueba UTF-8; si hay caracteres invalidos, se usa la
	// codificacion local de Windows.
	QString text = QString::fromUtf8(bytes);
	if (text.contains(QChar(0xFFFD)))
		text = QString::fromLocal8Bit(bytes);
	if (text.startsWith(QChar(0xFEFF)))
		text.remove(0, 1);

	static const QRegularExpression labelRe(QStringLiteral("^\\[[^\\]]*\\]$"));

	QStringList kept;
	const QStringList lines = normalizeNewlines(text).split(QLatin1Char('\n'));
	for (const QString &raw : lines) {
		const QString line = raw.trimmed();
		if (line.startsWith(QStringLiteral("//")))
			continue; // acordes o comentarios
		if (labelRe.match(line).hasMatch()) {
			kept << QString(); // la etiqueta separa estrofas
			continue;
		}
		kept << line;
	}

	const QStringList stanzas = splitStanzas(kept.join(QLatin1Char('\n')));

	Song result;
result.id = newId();
QString baseName = QFileInfo(path).completeBaseName();
QRegularExpression artistRe(QStringLiteral("^(.*?)\\s*\\((.*?)\\)$"));
QRegularExpressionMatch match = artistRe.match(baseName);
if (match.hasMatch()) {
	result.title = match.captured(1).trimmed();
	result.artist = match.captured(2).trimmed();
} else {
	result.title = baseName.trimmed();
}
for (const QString &s : stanzas) {
	Stanza st = parseStanza(s);
	result.stanzas.append(st);
	}
	if (result.stanzas.isEmpty())
		return false;

	song = result;
	return true;
}
