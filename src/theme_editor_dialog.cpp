#include "theme_editor_dialog.h"

#include "store.h"
#include "text_renderer.h"

#include <QAbstractButton>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace {

// Pinta el boton con el color elegido.
void setButtonColor(QPushButton *button, const QColor &color)
{
	button->setStyleSheet(
		QStringLiteral(
			"background-color: %1; border: 1px solid #888888; min-width: 40px; min-height: 22px;")
			.arg(color.name(QColor::HexRgb)));
}

// Dibuja un icono de 24 por 24 con lineas claras.
QIcon drawIcon(const std::function<void(QPainter &)> &draw)
{
	QPixmap pm(24, 24);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(QPen(QColor(230, 230, 230), 2));
	draw(p);
	p.end();
	return QIcon(pm);
}

// 0 izquierda, 1 centro, 2 derecha, 3 justificado
QIcon hAlignIcon(int kind)
{
	return drawIcon([kind](QPainter &p) {
		const int rows[4] = {5, 10, 15, 20};
		const int widths[4] = {18, 12, 18, 10};
		for (int i = 0; i < 4; ++i) {
			const int width = (kind == 3) ? 18 : widths[i];
			int x = 3;
			if (kind == 1)
				x = 3 + (18 - width) / 2;
			else if (kind == 2)
				x = 3 + (18 - width);
			p.drawLine(x, rows[i], x + width, rows[i]);
		}
	});
}

// 0 arriba, 1 medio, 2 abajo
QIcon vAlignIcon(int kind)
{
	return drawIcon([kind](QPainter &p) {
		QPen thin(QColor(150, 150, 150), 1);
		p.setPen(thin);
		p.drawRect(3, 3, 18, 18);
		p.setPen(QPen(QColor(230, 230, 230), 3));
		const int y = (kind == 0) ? 7 : (kind == 1 ? 12 : 17);
		p.drawLine(7, y, 17, y);
	});
}

QIcon marginIcon()
{
	return drawIcon([](QPainter &p) {
		p.drawRect(3, 3, 18, 18);
		p.setPen(QPen(QColor(230, 230, 230), 1));
		p.drawRect(8, 8, 8, 8);
	});
}

QToolButton *makeToolButton(QWidget *parent, const QIcon &icon, const QString &tip)
{
	auto *button = new QToolButton(parent);
	button->setIcon(icon);
	button->setIconSize(QSize(20, 20));
	button->setCheckable(true);
	button->setToolTip(tip);
	return button;
}

QSpinBox *makeSpin(QWidget *parent, int min, int max, const QString &suffix)
{
	auto *spin = new QSpinBox(parent);
	spin->setRange(min, max);
	spin->setSuffix(suffix);
	return spin;
}

// Pone una etiqueta arriba de un control, en la columna col de la grilla.
void addLabeled(QGridLayout *grid, int col, const QString &label, QWidget *widget)
{
	auto *text = new QLabel(label, widget->parentWidget());
	text->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
	grid->addWidget(text, 0, col);
	grid->addWidget(widget, 1, col);
}

} // namespace

// ---------------------------------------------------------------------------
// MarginPopup
// ---------------------------------------------------------------------------

MarginPopup::MarginPopup(QWidget *parent) : QDialog(parent, Qt::Tool)
{
	setWindowTitle(QStringLiteral("Margen"));

	auto *grid = new QGridLayout(this);

	auto makeMarginSpin = [this]() {
		auto *spin = new QSpinBox(this);
		spin->setRange(0, 45);
		spin->setSuffix(QStringLiteral(" %"));
		connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
			[this](int) {
				if (!loading_)
					emit changed();
			});
		return spin;
	};
	top_ = makeMarginSpin();
	bottom_ = makeMarginSpin();
	left_ = makeMarginSpin();
	right_ = makeMarginSpin();

	// Icono del centro: un cuadro con una A, como en HolyRics.
	QPixmap pm(48, 48);
	pm.fill(Qt::transparent);
	{
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(QPen(QColor(230, 230, 230), 2));
		p.drawRect(6, 6, 36, 36);
		QFont f = p.font();
		f.setPixelSize(24);
		f.setBold(true);
		p.setFont(f);
		p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("A"));
	}
	auto *icon = new QLabel(this);
	icon->setPixmap(pm);

	grid->addWidget(top_, 0, 1);
	grid->addWidget(left_, 1, 0);
	grid->addWidget(icon, 1, 1, Qt::AlignCenter);
	grid->addWidget(right_, 1, 2);
	grid->addWidget(bottom_, 2, 1);
}

void MarginPopup::setValues(int top, int bottom, int left, int right)
{
	loading_ = true;
	top_->setValue(top);
	bottom_->setValue(bottom);
	left_->setValue(left);
	right_->setValue(right);
	loading_ = false;
}

int MarginPopup::top() const
{
	return top_->value();
}

int MarginPopup::bottom() const
{
	return bottom_->value();
}

int MarginPopup::left() const
{
	return left_->value();
}

int MarginPopup::right() const
{
	return right_->value();
}

void MarginPopup::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	emit changed();
}

void MarginPopup::hideEvent(QHideEvent *event)
{
	QDialog::hideEvent(event);
	emit changed();
}

// ---------------------------------------------------------------------------
// ThemeEditorDialog
// ---------------------------------------------------------------------------

ThemeEditorDialog::ThemeEditorDialog(const Theme &theme, QWidget *parent)
	: QDialog(parent), theme_(theme)
{
	setWindowTitle(QStringLiteral("Editar tema"));
	resize(1100, 820);
	buildUi();
	loadControls();
	updatePreview();
}

void ThemeEditorDialog::buildUi()
{
	auto *root = new QVBoxLayout(this);

	// Parte de arriba: nombre, pestañas y botones
	auto *top = new QHBoxLayout();
	root->addLayout(top);

	auto *nameColumn = new QVBoxLayout();
	nameColumn->addWidget(new QLabel(QStringLiteral("Nombre del tema"), this));
	name_ = new QLineEdit(this);
	nameColumn->addWidget(name_);
	nameColumn->addStretch(1);
	top->addLayout(nameColumn);

	tabs_ = new QTabWidget(this);
	tabs_->addTab(buildFontTab(), QStringLiteral("Fuente/Alinear"));
	tabs_->addTab(buildEffectTab(), QStringLiteral("Efecto"));
	tabs_->addTab(buildBackgroundTab(), QStringLiteral("Color de fondo"));
	tabs_->addTab(buildBoxTab(), QStringLiteral("Contorno"));
	top->addWidget(tabs_, 1);

	auto *buttonColumn = new QVBoxLayout();
	auto *save = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton),
				     QStringLiteral("Guardar"), this);
	auto *cancel = new QPushButton(QStringLiteral("Cancelar"), this);
	buttonColumn->addWidget(save);
	buttonColumn->addWidget(cancel);
	buttonColumn->addStretch(1);
	top->addLayout(buttonColumn);
	connect(save, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

	// Vista previa grande (16:9)
	preview_ = new QLabel(this);
	preview_->setFixedSize(800, 450);
	root->addWidget(preview_, 1, Qt::AlignCenter);

	marginPopup_ = new MarginPopup(this);
	connect(marginPopup_, &MarginPopup::changed, this, [this]() { readControls(); });
	connect(name_, &QLineEdit::textChanged, this,
		[this](const QString &) { readControls(); });
}

QWidget *ThemeEditorDialog::buildFontTab()
{
	auto *tab = new QWidget();
	auto *row = new QHBoxLayout(tab);

	// Izquierda: fuente, color, tamaño, negrita, cursiva y espaciado
	auto *left = new QVBoxLayout();

	auto *line1 = new QHBoxLayout();
	font_ = new QFontComboBox(tab);
	colorButton_ = new QPushButton(tab);
	colorButton_->setToolTip(QStringLiteral("Color de la letra"));
	size_ = new QDoubleSpinBox(tab);
	size_->setRange(1.0, 40.0);
	size_->setSingleStep(0.5);
	size_->setDecimals(1);
	size_->setSuffix(QStringLiteral(" %"));
	size_->setToolTip(QStringLiteral("Tamaño de la letra, en porcentaje de la altura del lienzo"));
	loadFontButton_ = new QPushButton(QStringLiteral("+"), tab);
	loadFontButton_->setToolTip(QStringLiteral("Cargar fuente (.ttf / .otf)"));
	line1->addWidget(font_, 1);
	line1->addWidget(loadFontButton_);
	line1->addWidget(colorButton_);
	line1->addWidget(size_);
	left->addLayout(line1);

	auto *line2 = new QHBoxLayout();
	bold_ = new QToolButton(tab);
	bold_->setText(QStringLiteral("N"));
	bold_->setCheckable(true);
	bold_->setToolTip(QStringLiteral("Negrita"));
	italic_ = new QToolButton(tab);
	italic_->setText(QStringLiteral("K"));
	italic_->setCheckable(true);
	italic_->setToolTip(QStringLiteral("Cursiva"));

	uppercase_ = new QToolButton(tab);
	uppercase_->setText(QStringLiteral("AA"));
	uppercase_->setCheckable(true);
	uppercase_->setToolTip(QStringLiteral("Forzar MAYÚSCULAS"));
	lineSpacing_ = makeSpin(tab, 50, 250, QStringLiteral(" %"));
	lineSpacing_->setToolTip(QStringLiteral("Separación entre líneas (100 es lo normal)"));
	charSpacing_ = makeSpin(tab, -10, 60, QStringLiteral(" px"));
	charSpacing_->setToolTip(QStringLiteral("Separación entre letras"));
	line2->addWidget(bold_);
	line2->addWidget(italic_);
	line2->addWidget(uppercase_);
	line2->addSpacing(8);
	line2->addWidget(new QLabel(QStringLiteral("Líneas"), tab));
	line2->addWidget(lineSpacing_);
	line2->addSpacing(8);
	line2->addWidget(new QLabel(QStringLiteral("Letras"), tab));
	line2->addWidget(charSpacing_);
	line2->addStretch(1);
	left->addLayout(line2);
	left->addStretch(1);

	// Derecha: alineación y margen
	auto *center = new QVBoxLayout();
	auto *alignRow = new QHBoxLayout();

	const QString hTips[4] = {QStringLiteral("Alinear a la izquierda"),
				  QStringLiteral("Centrar"),
				  QStringLiteral("Alinear a la derecha"),
				  QStringLiteral("Justificar")};
	hGroup_ = new QButtonGroup(tab);
	for (int i = 0; i < 4; ++i) {
		QToolButton *button = makeToolButton(tab, hAlignIcon(i), hTips[i]);
		hGroup_->addButton(button, i);
		alignRow->addWidget(button);
	}
	alignRow->addSpacing(12);

	const QString vTips[3] = {QStringLiteral("Alinear arriba"),
				  QStringLiteral("Alinear al medio"),
				  QStringLiteral("Alinear abajo")};
	vGroup_ = new QButtonGroup(tab);
	for (int i = 0; i < 3; ++i) {
		QToolButton *button = makeToolButton(tab, vAlignIcon(i), vTips[i]);
		vGroup_->addButton(button, i);
		alignRow->addWidget(button);
	}
	alignRow->addStretch(1);
	center->addLayout(alignRow);

	marginButton_ = new QPushButton(marginIcon(), QStringLiteral("Margen"), tab);
	center->addWidget(marginButton_, 0, Qt::AlignLeft);
	center->addStretch(1);

	row->addLayout(left, 2);
	row->addSpacing(16);
	row->addLayout(center, 1);

	// Conexiones
	connect(font_, &QFontComboBox::currentFontChanged, this,
	[this](const QFont &) { readControls(); });
	connect(loadFontButton_, &QPushButton::clicked, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(
			this, QStringLiteral("Cargar fuente"), QString(),
			QStringLiteral("Fuentes (*.ttf *.otf)"));
		if (file.isEmpty())
			return;

		QFileInfo fi(file);
		const QString destPath = Store::instance().fontsPath() + "/" + fi.fileName();
		if (QFile::exists(destPath))
			QFile::remove(destPath);
		QFile::copy(file, destPath);

		int id = QFontDatabase::addApplicationFont(destPath);
		if (id != -1) {
			QStringList families = QFontDatabase::applicationFontFamilies(id);
			if (!families.isEmpty()) {
				font_->setCurrentFont(QFont(families.first()));
				readControls();
			}
		}
	});
	connect(size_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
		[this](double) { readControls(); });
	textTransparent_ = new QCheckBox(QStringLiteral("Letra transparente (perfora el fondo)"), tab);
	line1->addWidget(textTransparent_);
	connectCheck(textTransparent_);

	connectColor(colorButton_, &Theme::color, QStringLiteral("Color de la letra"));
	connectCheck(bold_);
	connectCheck(italic_);
	connectCheck(uppercase_);
	connectSpin(lineSpacing_);
	connectSpin(charSpacing_);
	connect(hGroup_, &QButtonGroup::idClicked, this, [this](int) { readControls(); });
	connect(vGroup_, &QButtonGroup::idClicked, this, [this](int) { readControls(); });
	connect(marginButton_, &QPushButton::clicked, this, [this]() {
		marginPopup_->move(
			marginButton_->mapToGlobal(QPoint(0, marginButton_->height())));
		marginPopup_->show();
		marginPopup_->raise();
	});
	return tab;
}

QWidget *ThemeEditorDialog::buildEffectTab()
{
	auto *tab = new QWidget();
	auto *row = new QHBoxLayout(tab);

	// Contorno de la letra
	auto *outlineBox = new QGroupBox(QStringLiteral("Contorno"), tab);
	auto *outlineGrid = new QGridLayout(outlineBox);
	outline_ = new QCheckBox(QStringLiteral("Activar"), outlineBox);
	outlineColorButton_ = new QPushButton(outlineBox);
	outlineWidth_ = makeSpin(outlineBox, 0, 40, QStringLiteral(" px"));
	outlineGrid->addWidget(outline_, 0, 0, 1, 2);
	outlineGrid->addWidget(outlineColorButton_, 1, 0);
	outlineGrid->addWidget(outlineWidth_, 1, 1);
	row->addWidget(outlineBox);

	// Brillo
	auto *glowBox = new QGroupBox(QStringLiteral("Brillo"), tab);
	auto *glowGrid = new QGridLayout(glowBox);
	glow_ = new QCheckBox(QStringLiteral("Activar"), glowBox);
	glowColorButton_ = new QPushButton(glowBox);
	glowSize_ = makeSpin(glowBox, 0, 80, QStringLiteral(" px"));
	glowGrid->addWidget(glow_, 0, 0, 1, 2);
	glowGrid->addWidget(glowColorButton_, 1, 0);
	glowGrid->addWidget(glowSize_, 1, 1);
	row->addWidget(glowBox);

	// Sombra: color, dirección (grilla de 3 por 3), distancia y desenfoque
	auto *shadowBox = new QGroupBox(QStringLiteral("Sombra"), tab);
	auto *shadowGrid = new QGridLayout(shadowBox);
	shadow_ = new QCheckBox(QStringLiteral("Activar"), shadowBox);
	shadowColorButton_ = new QPushButton(shadowBox);

	auto *dirHost = new QWidget(shadowBox);
	auto *dirGrid = new QGridLayout(dirHost);
	dirGrid->setContentsMargins(0, 0, 0, 0);
	dirGrid->setSpacing(2);
	shadowDirGroup_ = new QButtonGroup(shadowBox);
	for (int i = 0; i < 9; ++i) {
		auto *button = new QToolButton(dirHost);
		button->setCheckable(true);
		button->setFixedSize(18, 18);
		button->setToolTip(QStringLiteral("Dirección de la sombra"));
		button->setStyleSheet(QStringLiteral(
			"QToolButton { border: 1px solid #777777; } "
			"QToolButton:checked { background-color: #2a82da; }"));
		shadowDirGroup_->addButton(button, i);
		dirGrid->addWidget(button, i / 3, i % 3);
	}

	shadowDistance_ = makeSpin(shadowBox, 0, 40, QStringLiteral(" px"));
	shadowDistance_->setToolTip(QStringLiteral("Distancia de la sombra"));
	shadowBlur_ = new QCheckBox(QStringLiteral("Desenfoque"), shadowBox);
	auto *reset = new QPushButton(QStringLiteral("Restablecer"), shadowBox);

	shadowGrid->addWidget(shadow_, 0, 0, 1, 2);
	shadowGrid->addWidget(shadowColorButton_, 1, 0);
	shadowGrid->addWidget(shadowDistance_, 1, 1);
	shadowGrid->addWidget(dirHost, 2, 0, 2, 1);
	shadowGrid->addWidget(shadowBlur_, 2, 1);
	shadowGrid->addWidget(reset, 3, 1);
	row->addWidget(shadowBox);
	row->addStretch(1);

	// Conexiones
	connectCheck(outline_);
	connectColor(outlineColorButton_, &Theme::outlineColor,
		     QStringLiteral("Color del contorno"));
	connectSpin(outlineWidth_);
	connectCheck(glow_);
	connectColor(glowColorButton_, &Theme::glowColor, QStringLiteral("Color del brillo"));
	connectSpin(glowSize_);
	connectCheck(shadow_);
	connectColor(shadowColorButton_, &Theme::shadowColor,
		     QStringLiteral("Color de la sombra"));
	connectSpin(shadowDistance_);
	connectCheck(shadowBlur_);
	connect(shadowDirGroup_, &QButtonGroup::idClicked, this,
		[this](int) { readControls(); });
	connect(reset, &QPushButton::clicked, this, [this]() {
		shadowDirGroup_->button(8)->setChecked(true);
		shadowDistance_->setValue(4);
		shadowBlur_->setChecked(false);
		readControls();
	});
	return tab;
}

QWidget *ThemeEditorDialog::buildBackgroundTab()
{
	auto *tab = new QWidget();
	auto *outer = new QHBoxLayout(tab);
	bgEnabled_ = new QCheckBox(QStringLiteral("Activar"), tab);
	outer->addWidget(bgEnabled_);

	auto *host = new QWidget(tab);
	auto *grid = new QGridLayout(host);
	bgColorButton_ = new QPushButton(host);
	bgOpacity_ = makeSpin(host, 0, 100, QStringLiteral(" %"));
	bgPadX_ = makeSpin(host, 0, 1000, QStringLiteral(" px"));
	bgPadY_ = makeSpin(host, 0, 1000, QStringLiteral(" px"));
	bgRadius_ = makeSpin(host, 0, 200, QStringLiteral(" px"));
	bgType_ = new QComboBox(host);
	bgType_->addItem(QStringLiteral("Una caja por línea"));
	bgType_->addItem(QStringLiteral("Una sola caja"));
	addLabeled(grid, 0, QStringLiteral("Color"), bgColorButton_);
	addLabeled(grid, 1, QStringLiteral("Opacidad"), bgOpacity_);
	addLabeled(grid, 2, QStringLiteral("Margen horizontal"), bgPadX_);
	addLabeled(grid, 3, QStringLiteral("Margen vertical"), bgPadY_);
	addLabeled(grid, 4, QStringLiteral("Esquina"), bgRadius_);
	addLabeled(grid, 5, QStringLiteral("Tipo"), bgType_);
	outer->addWidget(host);
	outer->addStretch(1);

	connectCheck(bgEnabled_);
	connectColor(bgColorButton_, &Theme::bgColor, QStringLiteral("Color de fondo"));
	connectSpin(bgOpacity_);
	connectSpin(bgPadX_);
	connectSpin(bgPadY_);
	connectSpin(bgRadius_);
	connectCombo(bgType_);
	return tab;
}

QWidget *ThemeEditorDialog::buildBoxTab()
{
	auto *tab = new QWidget();
	auto *outer = new QHBoxLayout(tab);
	boxEnabled_ = new QCheckBox(QStringLiteral("Activar"), tab);
	outer->addWidget(boxEnabled_);

	auto *host = new QWidget(tab);
	auto *grid = new QGridLayout(host);
boxFillColorButton_ = new QPushButton(host);
boxFillOpacity_ = makeSpin(host, 0, 100, QStringLiteral(" %"));
boxPadX_ = makeSpin(host, 0, 1000, QStringLiteral(" px"));
boxPadY_ = makeSpin(host, 0, 1000, QStringLiteral(" px"));
boxRadius_ = makeSpin(host, 0, 200, QStringLiteral(" px"));
	boxType_ = new QComboBox(host);
	boxType_->addItem(QStringLiteral("Un borde por línea"));
	boxType_->addItem(QStringLiteral("Un solo borde"));
	boxWidthMode_ = new QComboBox(host);
	boxWidthMode_->addItem(QStringLiteral("Ajustar al texto"));
	boxWidthMode_->addItem(QStringLiteral("Ajustar a los márgenes"));
	addLabeled(grid, 0, QStringLiteral("Color relleno"), boxFillColorButton_);
	addLabeled(grid, 1, QStringLiteral("Opacidad relleno"), boxFillOpacity_);
	addLabeled(grid, 2, QStringLiteral("Margen horizontal"), boxPadX_);
	addLabeled(grid, 3, QStringLiteral("Margen vertical"), boxPadY_);
	addLabeled(grid, 4, QStringLiteral("Esquina"), boxRadius_);
	addLabeled(grid, 5, QStringLiteral("Tipo"), boxType_);
	addLabeled(grid, 6, QStringLiteral("Ancho caja"), boxWidthMode_);
	outer->addWidget(host);
	outer->addStretch(1);

	connectCheck(boxEnabled_);
	connectColor(boxFillColorButton_, &Theme::boxFillColor, QStringLiteral("Color de relleno"));
	connectSpin(boxFillOpacity_);
	connectSpin(boxPadX_);
	connectSpin(boxPadY_);
	connectSpin(boxRadius_);
	connectCombo(boxType_);
	connectCombo(boxWidthMode_);
	return tab;
}

void ThemeEditorDialog::connectSpin(QSpinBox *spin)
{
	connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int) { readControls(); });
}

void ThemeEditorDialog::connectCheck(QAbstractButton *button)
{
	connect(button, &QAbstractButton::toggled, this, [this](bool) { readControls(); });
}

void ThemeEditorDialog::connectCombo(QComboBox *combo)
{
	connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int) { readControls(); });
}

void ThemeEditorDialog::connectColor(QPushButton *button, QColor Theme::*member,
				     const QString &title)
{
	connect(button, &QPushButton::clicked, this, [this, button, member, title]() {
		const QColor c = QColorDialog::getColor(theme_.*member, this, title,
							QColorDialog::ShowAlphaChannel);
		if (!c.isValid())
			return;
		theme_.*member = c;
		setButtonColor(button, c);
		updatePreview();
	});
}

// Copia el tema a los controles, sin disparar cambios.
void ThemeEditorDialog::loadControls()
{
	loading_ = true;

	name_->setText(theme_.name);

	font_->setCurrentFont(QFont(theme_.fontFamily));
	setButtonColor(colorButton_, theme_.color);
	size_->setValue(theme_.fontSize);
	bold_->setChecked(theme_.bold);
	italic_->setChecked(theme_.italic);
	uppercase_->setChecked(theme_.uppercase);
	lineSpacing_->setValue(theme_.lineSpacing);
	charSpacing_->setValue(theme_.charSpacing);
	if (QAbstractButton *b = hGroup_->button(qBound(0, theme_.hAlign, 3)))
		b->setChecked(true);
	if (QAbstractButton *b = vGroup_->button(qBound(0, theme_.vAlign, 2)))
		b->setChecked(true);
	marginPopup_->setValues(theme_.marginTop, theme_.marginBottom, theme_.marginLeft,
				theme_.marginRight);

	outline_->setChecked(theme_.outline);
	setButtonColor(outlineColorButton_, theme_.outlineColor);
	outlineWidth_->setValue(theme_.outlineWidth);
	glow_->setChecked(theme_.glow);
	setButtonColor(glowColorButton_, theme_.glowColor);
	glowSize_->setValue(theme_.glowSize);
	shadow_->setChecked(theme_.shadow);
	setButtonColor(shadowColorButton_, theme_.shadowColor);
	if (QAbstractButton *b = shadowDirGroup_->button(qBound(0, theme_.shadowDir, 8)))
		b->setChecked(true);
	shadowDistance_->setValue(theme_.shadowDistance);
	shadowBlur_->setChecked(theme_.shadowBlur);

	bgEnabled_->setChecked(theme_.bgEnabled);
	setButtonColor(bgColorButton_, theme_.bgColor);
	bgOpacity_->setValue(theme_.bgOpacity);
	bgPadX_->setValue(theme_.bgPadX);
	bgPadY_->setValue(theme_.bgPadY);
	bgRadius_->setValue(theme_.bgRadius);
	bgType_->setCurrentIndex(qBound(0, theme_.bgType, 1));

	boxEnabled_->setChecked(theme_.boxEnabled);
	setButtonColor(boxFillColorButton_, theme_.boxFillColor);
	boxFillOpacity_->setValue(theme_.boxFillOpacity);
	textTransparent_->setChecked(theme_.textTransparent);
	boxPadX_->setValue(theme_.boxPadX);
	boxPadY_->setValue(theme_.boxPadY);
	boxRadius_->setValue(theme_.boxRadius);
	boxType_->setCurrentIndex(qBound(0, theme_.boxType, 1));
	boxWidthMode_->setCurrentIndex(qBound(0, theme_.boxWidthMode, 1));

	loading_ = false;
}

// Copia los controles al tema y actualiza la vista previa.
void ThemeEditorDialog::readControls()
{
	if (loading_)
		return;

	theme_.name = name_->text().trimmed();
	if (theme_.name.isEmpty())
		theme_.name = QStringLiteral("Tema");

	theme_.fontFamily = font_->currentFont().family();
	theme_.fontSize = size_->value();
	theme_.bold = bold_->isChecked();
	theme_.italic = italic_->isChecked();
	theme_.uppercase = uppercase_->isChecked();
	theme_.lineSpacing = lineSpacing_->value();
	theme_.charSpacing = charSpacing_->value();
	if (hGroup_->checkedId() >= 0)
		theme_.hAlign = hGroup_->checkedId();
	if (vGroup_->checkedId() >= 0)
		theme_.vAlign = vGroup_->checkedId();
	theme_.marginTop = marginPopup_->top();
	theme_.marginBottom = marginPopup_->bottom();
	theme_.marginLeft = marginPopup_->left();
	theme_.marginRight = marginPopup_->right();

	theme_.outline = outline_->isChecked();
	theme_.outlineWidth = outlineWidth_->value();
	theme_.glow = glow_->isChecked();
	theme_.glowSize = glowSize_->value();
	theme_.shadow = shadow_->isChecked();
	if (shadowDirGroup_->checkedId() >= 0)
		theme_.shadowDir = shadowDirGroup_->checkedId();
	theme_.shadowDistance = shadowDistance_->value();
	theme_.shadowBlur = shadowBlur_->isChecked();

	theme_.bgEnabled = bgEnabled_->isChecked();
	theme_.bgOpacity = bgOpacity_->value();
	theme_.bgPadX = bgPadX_->value();
	theme_.bgPadY = bgPadY_->value();
	theme_.bgRadius = bgRadius_->value();
	theme_.bgType = bgType_->currentIndex();

	theme_.boxEnabled = boxEnabled_->isChecked();
	theme_.boxFillOpacity = boxFillOpacity_->value();
	theme_.textTransparent = textTransparent_->isChecked();
	theme_.boxPadX = boxPadX_->value();
	theme_.boxPadY = boxPadY_->value();
	theme_.boxRadius = boxRadius_->value();
	theme_.boxType = boxType_->currentIndex();
	theme_.boxWidthMode = boxWidthMode_->currentIndex();

	updatePreview();
}

void ThemeEditorDialog::updatePreview()
{
	const int w = 800;
	const int h = 450;
	QImage img = renderStanza(
		theme_,
		QStringLiteral("Texto de ejemplo\nAA BB CC XX YY ZZ\n1 2 3 4 5 6 7 8 9 0\nÁÉÍÓÚ ÃÊ"),
		100, w, h, QColor(32, 32, 32));

	// Mientras la ventana Margen está abierta, se marca el área del texto.
	if (marginPopup_ && marginPopup_->isVisible()) {
		QPainter painter(&img);
		painter.setPen(QPen(QColor(255, 255, 255), 2, Qt::DashLine));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(textArea(theme_, w, h));
		painter.end();
	}
	preview_->setPixmap(QPixmap::fromImage(img));
}