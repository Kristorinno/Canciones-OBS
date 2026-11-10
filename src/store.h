#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include "data_model.h"

// Datos del plugin que se guardan como archivos en la carpeta de
// configuracion del plugin. Solo se usa desde el hilo de la interfaz.
class Store {
public:
	static Store &instance();

	void load();

	void saveSongs();
	void savePlaylist();
	void saveThemes();
	void saveSettings();

	Song *findSong(const QString &id);
	Theme *findTheme(const QString &id);
	Theme currentTheme();
	void sortSongs();

	QVector<Song> songs;
	QStringList playlist; // ids de canciones, en orden
	QVector<Theme> themes;
	QString selectedThemeId;
	TransitionSettings transition;
	QByteArray windowGeometry;
QByteArray splitterState;
int themeThumbWidth = 220;
int defaultCardFontSize = 10; // Tamaño global por defecto de las tarjetas

	QString fontsPath() const;
};
