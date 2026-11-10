#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// Identificador unico para canciones y temas.
QString newId();

// Minusculas y sin tildes, para buscar sin importar los acentos.
QString foldText(const QString &s);

struct Stanza {
	QString text;
	int fontScale = 100; // porcentaje sobre el tamanio de letra del tema
	int cardFontSize = -1; // -1 significa que usa el tamaño global
};

struct Song {
	QString id;
	QString title;
	QString artist;
	QVector<Stanza> stanzas;
	int defaultCardFontSize = -1; // -1 significa usar el global del plugin

	// Texto de todas las estrofas juntas (para el buscador).
	QString allText() const;
};

// Un tema define como se ve la letra. Las medidas "px" estan pensadas para
// un lienzo de 1080 de alto y se escalan solas a otros tamanios.
struct Theme {
	QString id;
	QString name = QStringLiteral("Tema");

	// Fuente / Alinear
	QString fontFamily = QStringLiteral("Arial");
	double fontSize = 8.0; // porcentaje de la altura del lienzo
	QColor color = QColor(255, 255, 255);
	bool bold = true;
	bool italic = false;
	bool uppercase = false;
	int charSpacing = 0;   // px
	int lineSpacing = 100; // porcentaje
	int hAlign = 1; // 0 izquierda, 1 centro, 2 derecha, 3 justificado
	int vAlign = 1; // 0 arriba, 1 medio, 2 abajo

	// Margen: espacio libre alrededor del texto, en porcentaje del lienzo.
	int marginTop = 5;
	int marginBottom = 5;
	int marginLeft = 5;
	int marginRight = 5;
	// Sin uso en la interfaz (siempre 0): corrimiento del area de texto.
	int offsetX = 0;
	int offsetY = 0;

	// Efecto: contorno de la letra
	bool outline = false;
	int outlineWidth = 4; // px
	QColor outlineColor = QColor(0, 0, 0);

	// Efecto: brillo
	bool glow = false;
	QColor glowColor = QColor(255, 255, 255, 200);
	int glowSize = 16; // px

	// Efecto: sombra
	bool shadow = true;
	QColor shadowColor = QColor(0, 0, 0, 200);
	int shadowDir = 8;      // 0 a 8 en una grilla de 3 por 3; 4 es el centro
	int shadowDistance = 4; // px
	bool shadowBlur = false;

	// Color de fondo: caja detras del texto
	bool bgEnabled = false;
	QColor bgColor = QColor(0, 0, 0);
	int bgOpacity = 60; // porcentaje
	int bgPadX = 24;    // px
	int bgPadY = 8;     // px
	int bgRadius = 0;   // px
	int bgType = 0;     // 0 una caja por linea, 1 una sola caja

	// Contorno: borde alrededor del texto
	bool boxEnabled = false;
	QColor boxFillColor = QColor(0, 0, 0);
	int boxFillOpacity = 100;
	bool textTransparent = false; // Hace un agujero en el recuadro
	int boxPadX = 24;     // px
	int boxPadY = 8;      // px
	int boxRadius = 0;    // px
	int boxType = 1;      // 0 un borde por linea, 1 un solo borde
	int boxWidthMode = 0; // 0 se ajusta al texto, 1 se ajusta a los margenes
};

struct TransitionSettings {
	// none, fade, crossfade, slide_up, slide_down, slide_left, slide_right, zoom
	QString type = QStringLiteral("crossfade");
	int durationMs = 400;
};

Theme defaultTheme();

QJsonObject songToJson(const Song &song);
Song songFromJson(const QJsonObject &obj);
QJsonObject themeToJson(const Theme &theme);
Theme themeFromJson(const QJsonObject &obj);