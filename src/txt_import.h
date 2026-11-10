#pragma once

#include <QString>
#include <QStringList>

#include "data_model.h"

// Parte un texto en estrofas: una linea en blanco separa una estrofa de otra.
QStringList splitStanzas(const QString &text);

// Lee el comando ## al principio de la estrofa para ajustar el tamaño.
Stanza parseStanza(const QString &text);

// Lee una cancion desde un archivo .txt.
// - El titulo es el nombre del archivo, sin la extension.
// - Las estrofas se separan con lineas en blanco.
// - Se ignoran las lineas que empiezan con // (acordes o comentarios).
// - Las lineas entre corchetes, como [VERSO 1] o [CORO], se tratan como
//   separador y no se incluyen en la letra.
// Devuelve false si no se pudo leer o si no tiene letra.
bool songFromTxtFile(const QString &path, Song &song);
