#include "store.h"

#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {

// Ruta de un archivo dentro de la carpeta de configuracion del plugin.
QString configPath(const QString &file)
{
	QString path;
	char *p = obs_module_config_path(file.toUtf8().constData());
	if (p) {
		path = QString::fromUtf8(p);
		bfree(p);
	}
	if (path.isEmpty()) {
		path = QStandardPaths::writableLocation(
			       QStandardPaths::AppDataLocation) +
		       QStringLiteral("/canciones-obs/") + file;
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	return path;
}

QJsonDocument readJson(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return QJsonDocument();
	return QJsonDocument::fromJson(f.readAll());
}

bool writeJson(const QString &path, const QJsonObject &obj)
{
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return f.commit();
}

const QString kSongsFile = QStringLiteral("canciones.json");
const QString kPlaylistFile = QStringLiteral("lista.json");
const QString kThemesFile = QStringLiteral("temas.json");
const QString kSettingsFile = QStringLiteral("ajustes.json");

} // namespace

Store &Store::instance()
{
	static Store store;
	return store;
}

void Store::load()
{
	songs.clear();
	playlist.clear();
	themes.clear();
	selectedThemeId.clear();

	// Canciones
	const QJsonArray songArr =
		readJson(configPath(kSongsFile)).object().value(QStringLiteral("songs")).toArray();
	for (const QJsonValue &v : songArr) {
		Song s = songFromJson(v.toObject());
		if (!s.id.isEmpty())
			songs.append(s);
	}
	sortSongs();

	// Lista de reproduccion
	const QJsonArray idArr =
		readJson(configPath(kPlaylistFile)).object().value(QStringLiteral("ids")).toArray();
	for (const QJsonValue &v : idArr) {
		const QString id = v.toString();
		if (findSong(id) && !playlist.contains(id))
			playlist.append(id);
	}

	// Temas
	const QJsonObject themesObj = readJson(configPath(kThemesFile)).object();
	const QJsonArray themeArr = themesObj.value(QStringLiteral("themes")).toArray();
	for (const QJsonValue &v : themeArr)
		themes.append(themeFromJson(v.toObject()));
	if (themes.isEmpty())
		themes.append(defaultTheme());
	selectedThemeId = themesObj.value(QStringLiteral("selected")).toString();
	if (!findTheme(selectedThemeId))
		selectedThemeId = themes.first().id;

	// Ajustes
	const QJsonObject settings = readJson(configPath(kSettingsFile)).object();
	const QJsonObject tr = settings.value(QStringLiteral("transition")).toObject();
	transition.type = tr.value(QStringLiteral("type")).toString(transition.type);
	transition.durationMs = tr.value(QStringLiteral("durationMs")).toInt(transition.durationMs);
	windowGeometry = QByteArray::fromBase64(
		settings.value(QStringLiteral("geometry")).toString().toLatin1());
	
	splitterState = QByteArray::fromBase64(
		settings.value(QStringLiteral("splitterState")).toString().toLatin1());
	themeThumbWidth = settings.value(QStringLiteral("themeThumbWidth")).toInt(220);
	defaultCardFontSize = settings.value(QStringLiteral("defaultCardFontSize")).toInt(10);
}

void Store::saveSongs()
{
	QJsonArray arr;
	for (const Song &s : songs)
		arr.append(songToJson(s));
	QJsonObject o;
	o.insert(QStringLiteral("songs"), arr);
	writeJson(configPath(kSongsFile), o);
}

void Store::savePlaylist()
{
	QJsonArray arr;
	for (const QString &id : playlist)
		arr.append(id);
	QJsonObject o;
	o.insert(QStringLiteral("ids"), arr);
	writeJson(configPath(kPlaylistFile), o);
}

void Store::saveThemes()
{
	QJsonArray arr;
	for (const Theme &t : themes)
		arr.append(themeToJson(t));
	QJsonObject o;
	o.insert(QStringLiteral("themes"), arr);
	o.insert(QStringLiteral("selected"), selectedThemeId);
	writeJson(configPath(kThemesFile), o);
}

void Store::saveSettings()
{
	QJsonObject tr;
	tr.insert(QStringLiteral("type"), transition.type);
	tr.insert(QStringLiteral("durationMs"), transition.durationMs);
	QJsonObject o;
	o.insert(QStringLiteral("transition"), tr);
	o.insert(QStringLiteral("geometry"),
		 QString::fromLatin1(windowGeometry.toBase64()));
	o.insert(QStringLiteral("splitterState"),
		 QString::fromLatin1(splitterState.toBase64()));
	o.insert(QStringLiteral("themeThumbWidth"), themeThumbWidth);
o.insert(QStringLiteral("defaultCardFontSize"), defaultCardFontSize);
writeJson(configPath(kSettingsFile), o);
}

Song *Store::findSong(const QString &id)
{
	for (Song &s : songs) {
		if (s.id == id)
			return &s;
	}
	return nullptr;
}

Theme *Store::findTheme(const QString &id)
{
	for (Theme &t : themes) {
		if (t.id == id)
			return &t;
	}
	return nullptr;
}

Theme Store::currentTheme()
{
	if (Theme *t = findTheme(selectedThemeId))
		return *t;
	if (!themes.isEmpty())
		return themes.first();
	return defaultTheme();
}

QString Store::fontsPath() const
{
	QString path;
	char *p = obs_module_config_path("fonts");
	if (p) {
		path = QString::fromUtf8(p);
		bfree(p);
	}
	QDir().mkpath(path);
	return path;
}

void Store::sortSongs()
{
	std::sort(songs.begin(), songs.end(), [](const Song &a, const Song &b) {
		return foldText(a.title) < foldText(b.title);
	});
}
