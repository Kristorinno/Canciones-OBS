#include "letter_size_dialog.h"

#include "text_renderer.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

LetterSizeDialog::LetterSizeDialog(const Theme &theme, const QString &text,
				   int scale, QWidget *parent)
	: QDialog(parent), theme_(theme), text_(text)
{
	setWindowTitle(QStringLiteral("Editor Letras"));

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(
		QStringLiteral(
			"Tamaño de la letra de esta estrofa (porcentaje del tamaño del tema)"),
		this));

	auto *row = new QHBoxLayout();
	slider_ = new QSlider(Qt::Horizontal, this);
	slider_->setRange(20, 400);
	spin_ = new QSpinBox(this);
	spin_->setRange(20, 400);
	spin_->setSingleStep(5);
	spin_->setSuffix(QStringLiteral(" %"));
	row->addWidget(slider_, 1);
	row->addWidget(spin_);
	layout->addLayout(row);

	preview_ = new QLabel(this);
	preview_->setFixedSize(480, 270);
	layout->addWidget(preview_, 0, Qt::AlignCenter);

	auto *buttons = new QDialogButtonBox(this);
	buttons->addButton(QStringLiteral("Aceptar"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
	layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(slider_, &QSlider::valueChanged, spin_, &QSpinBox::setValue);
	connect(spin_, QOverload<int>::of(&QSpinBox::valueChanged), slider_,
		&QSlider::setValue);
	connect(spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int) { updatePreview(); });

	spin_->setValue(qBound(20, scale, 400));
	slider_->setValue(spin_->value());
	updatePreview();
}

int LetterSizeDialog::scale() const
{
	return spin_->value();
}

void LetterSizeDialog::updatePreview()
{
	const QImage img = renderStanza(theme_, text_, spin_->value(), 480, 270,
					QColor(32, 32, 32));
	preview_->setPixmap(QPixmap::fromImage(img));
}
