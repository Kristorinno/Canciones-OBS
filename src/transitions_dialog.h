#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>

#include "data_model.h"

class QComboBox;
class QLabel;
class QSpinBox;
class QTimer;

// Ventana "Transiciones": elige el tipo y la duracion de la transicion entre
// estrofas, con una vista previa animada.
class TransitionsDialog : public QDialog {
	Q_OBJECT

public:
	TransitionsDialog(const TransitionSettings &settings, const Theme &theme,
			  QWidget *parent = nullptr);

	TransitionSettings settings() const;

private:
	void tick();

	QComboBox *type_ = nullptr;
	QSpinBox *duration_ = nullptr;
	QLabel *preview_ = nullptr;
	QTimer *timer_ = nullptr;
	QElapsedTimer clock_;
	QImage imgA_;
	QImage imgB_;
};
