#pragma once

#include <QColor>
#include <QDialog>

#include "data_model.h"

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSpinBox;
class QTabWidget;
class QToolButton;

// Ventana flotante "Margen": cuatro valores (arriba, izquierda, derecha y
// abajo) alrededor de un icono, en porcentaje del lienzo.
class MarginPopup : public QDialog {
	Q_OBJECT

public:
	explicit MarginPopup(QWidget *parent = nullptr);

	void setValues(int top, int bottom, int left, int right);
	int top() const;
	int bottom() const;
	int left() const;
	int right() const;

signals:
	// Cambio un valor, o la ventana se mostro u oculto.
	void changed();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	QSpinBox *top_ = nullptr;
	QSpinBox *bottom_ = nullptr;
	QSpinBox *left_ = nullptr;
	QSpinBox *right_ = nullptr;
	bool loading_ = false;
};

// Editor de un tema, con el mismo esquema que HolyRics: pestañas Fuente/Alinear,
// Efecto, Color de fondo y Contorno, la ventana Margen y una vista previa
// grande con el area de texto marcada.
class ThemeEditorDialog : public QDialog {
	Q_OBJECT

public:
	explicit ThemeEditorDialog(const Theme &theme, QWidget *parent = nullptr);

	Theme theme() const { return theme_; }

private:
	void buildUi();
	QWidget *buildFontTab();
	QWidget *buildEffectTab();
	QWidget *buildBackgroundTab();
	QWidget *buildBoxTab();

	void connectSpin(QSpinBox *spin);
	void connectCheck(QAbstractButton *button);
	void connectCombo(QComboBox *combo);
	void connectColor(QPushButton *button, QColor Theme::*member,
			  const QString &title);

	void loadControls();
	void readControls();
	void updatePreview();

	Theme theme_;
	bool loading_ = false;

	QLineEdit *name_ = nullptr;
	QTabWidget *tabs_ = nullptr;
	QLabel *preview_ = nullptr;
	MarginPopup *marginPopup_ = nullptr;

	// Fuente / Alinear
	QFontComboBox *font_ = nullptr;
	QPushButton *loadFontButton_ = nullptr;
	QPushButton *colorButton_ = nullptr;
	QDoubleSpinBox *size_ = nullptr;
	QToolButton *bold_ = nullptr;
	QToolButton *italic_ = nullptr;
	QToolButton *uppercase_ = nullptr;
	QSpinBox *lineSpacing_ = nullptr;
	QSpinBox *charSpacing_ = nullptr;
	QButtonGroup *hGroup_ = nullptr;
	QButtonGroup *vGroup_ = nullptr;
	QPushButton *marginButton_ = nullptr;

	// Efecto
	QCheckBox *outline_ = nullptr;
	QPushButton *outlineColorButton_ = nullptr;
	QSpinBox *outlineWidth_ = nullptr;
	QCheckBox *glow_ = nullptr;
	QPushButton *glowColorButton_ = nullptr;
	QSpinBox *glowSize_ = nullptr;
	QCheckBox *shadow_ = nullptr;
	QPushButton *shadowColorButton_ = nullptr;
	QButtonGroup *shadowDirGroup_ = nullptr;
	QSpinBox *shadowDistance_ = nullptr;
	QCheckBox *shadowBlur_ = nullptr;

	// Color de fondo
	QCheckBox *bgEnabled_ = nullptr;
	QPushButton *bgColorButton_ = nullptr;
	QSpinBox *bgOpacity_ = nullptr;
	QSpinBox *bgPadX_ = nullptr;
	QSpinBox *bgPadY_ = nullptr;
	QSpinBox *bgRadius_ = nullptr;
	QComboBox *bgType_ = nullptr;

	// Contorno (borde)
	QCheckBox *boxEnabled_ = nullptr;
	QPushButton *boxFillColorButton_ = nullptr;
	QSpinBox *boxFillOpacity_ = nullptr;
	QCheckBox *textTransparent_ = nullptr;
	QSpinBox *boxPadX_ = nullptr;
	QSpinBox *boxPadY_ = nullptr;
	QSpinBox *boxRadius_ = nullptr;
	QComboBox *boxType_ = nullptr;
	QComboBox *boxWidthMode_ = nullptr;
};