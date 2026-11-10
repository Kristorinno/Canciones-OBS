#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>

#include "data_model.h"

// Area del lienzo donde se escribe el texto (dentro de los margenes y
// con el desplazamiento del tema).
QRectF textArea(const Theme &theme, int width, int height);

// Dibuja una estrofa segun el tema, en una imagen de width por height.
// El fondo es transparente salvo que se indique otro color.
QImage renderStanza(const Theme &theme, const QString &text,
		    int fontScalePercent, int width, int height,
		    const QColor &background = QColor(0, 0, 0, 0));
