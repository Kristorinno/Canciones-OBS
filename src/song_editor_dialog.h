#pragma once

#include <QDialog>
#include <QPixmap>
#include <QVector>

#include "data_model.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTimer;
class QWidget;

// Ventana "Editar Canciones": sirve para agregar canciones nuevas y para
// editar las existentes. A la derecha muestra una vista previa de cada
// estrofa, con un boton para ajustar el tamanio de su letra.
class SongEditorDialog : public QDialog {
	Q_OBJECT

public:
	SongEditorDialog(const Song &song, bool isNew, QWidget *parent = nullptr);

	Song song() const { return song_; }

	void accept() override;

private:
	void rebuildStanzas();
	void rebuildCards();
	void editStanzaSize(int index);
	QPixmap previewFor(int index) const;

	Song song_;
	Theme theme_;
	QLineEdit *title_ = nullptr;
	QLineEdit *artist_ = nullptr;
	QPlainTextEdit *lyrics_ = nullptr;
	QWidget *cardsHost_ = nullptr;
	QGridLayout *grid_ = nullptr;
	QTimer *timer_ = nullptr;
	QVector<QLabel *> previews_;
};
