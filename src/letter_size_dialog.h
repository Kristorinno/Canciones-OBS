#pragma once

#include <QDialog>
#include <QString>

#include "data_model.h"

class QLabel;
class QSlider;
class QSpinBox;

// Ventana "Editor Letras": ajusta el tamanio de la letra de una estrofa.
// Por ahora solo tiene esa opcion; mas adelante se le pueden sumar otras.
class LetterSizeDialog : public QDialog {
	Q_OBJECT

public:
	LetterSizeDialog(const Theme &theme, const QString &text, int scale,
			 QWidget *parent = nullptr);

	// Porcentaje elegido sobre el tamanio de letra del tema.
	int scale() const;

private:
	void updatePreview();

	Theme theme_;
	QString text_;
	QSpinBox *spin_ = nullptr;
	QSlider *slider_ = nullptr;
	QLabel *preview_ = nullptr;
};
