#include "transitions_dialog.h"

#include "text_renderer.h"
#include "transition_math.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

struct TypeEntry {
	const char *id;
	const char *label;
};

const TypeEntry kTypes[] = {
	{"none", "Sin transición (corte directo)"},
	{"fade", "Desvanecimiento"},
	{"crossfade", "Fundido cruzado"},
	{"slide_up", "Deslizar hacia arriba con desvanecimiento"},
	{"slide_down", "Deslizar hacia abajo con desvanecimiento"},
	{"slide_left", "Deslizar hacia la izquierda con desvanecimiento"},
	{"slide_right", "Deslizar hacia la derecha con desvanecimiento"},
	{"zoom", "Zoom con desvanecimiento"},
};

const int kPreviewW = 480;
const int kPreviewH = 270;

} // namespace

TransitionsDialog::TransitionsDialog(const TransitionSettings &settings,
				     const Theme &theme, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Transiciones"));

	auto *layout = new QVBoxLayout(this);
	auto *form = new QFormLayout();

	type_ = new QComboBox(this);
	int selected = 0;
	int index = 0;
	for (const TypeEntry &t : kTypes) {
		type_->addItem(QString::fromUtf8(t.label), QString::fromUtf8(t.id));
		if (settings.type == QLatin1String(t.id))
			selected = index;
		++index;
	}
	type_->setCurrentIndex(selected);
	form->addRow(QStringLiteral("Tipo de transición"), type_);

	duration_ = new QSpinBox(this);
	duration_->setRange(100, 3000);
	duration_->setSingleStep(50);
	duration_->setSuffix(QStringLiteral(" ms"));
	duration_->setValue(qBound(100, settings.durationMs, 3000));
	form->addRow(QStringLiteral("Duración"), duration_);
	layout->addLayout(form);

	layout->addWidget(new QLabel(QStringLiteral("Vista previa"), this));
	preview_ = new QLabel(this);
	preview_->setFixedSize(kPreviewW, kPreviewH);
	layout->addWidget(preview_, 0, Qt::AlignCenter);

	auto *buttons = new QDialogButtonBox(this);
	buttons->addButton(QStringLiteral("Aceptar"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	imgA_ = renderStanza(theme, QStringLiteral("Primera estrofa\nde prueba"), 100,
			     kPreviewW, kPreviewH);
	imgB_ = renderStanza(theme, QStringLiteral("Segunda estrofa\nde prueba"), 100,
			     kPreviewW, kPreviewH);

	clock_.start();
	timer_ = new QTimer(this);
	timer_->setInterval(16);
	connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
	timer_->start();
	tick();
}

TransitionSettings TransitionsDialog::settings() const
{
	TransitionSettings s;
	s.type = type_->currentData().toString();
	s.durationMs = duration_->value();
	return s;
}

// Muestra en bucle: estrofa A, transicion a B, estrofa B, transicion a A.
void TransitionsDialog::tick()
{
	const QString type = type_->currentData().toString();
	const double duration = duration_->value();
	const double hold = 700.0;
	const double period = 2.0 * (hold + duration);
	const double t = std::fmod(double(clock_.elapsed()), period);

	QImage frame(kPreviewW, kPreviewH, QImage::Format_ARGB32_Premultiplied);
	frame.fill(QColor(32, 32, 32));
	QPainter painter(&frame);

	auto drawImage = [&](const QImage &img, double opacity, double dx,
			     double dy, double zoom) {
		painter.save();
		painter.setOpacity(opacity);
		painter.translate(kPreviewW / 2.0 + dx, kPreviewH / 2.0 + dy);
		painter.scale(zoom, zoom);
		painter.translate(-kPreviewW / 2.0, -kPreviewH / 2.0);
		painter.drawImage(0, 0, img);
		painter.restore();
	};

	if (t < hold) {
		drawImage(imgA_, 1.0, 0.0, 0.0, 1.0);
	} else if (t < hold + duration) {
		const TransitionParams tp = computeTransition(
			type, (t - hold) / duration, kPreviewW, kPreviewH);
		drawImage(imgA_, tp.oldOpacity, tp.oldDx, tp.oldDy, tp.oldScale);
		drawImage(imgB_, tp.newOpacity, tp.newDx, tp.newDy, tp.newScale);
	} else if (t < 2.0 * hold + duration) {
		drawImage(imgB_, 1.0, 0.0, 0.0, 1.0);
	} else {
		const TransitionParams tp = computeTransition(
			type, (t - (2.0 * hold + duration)) / duration, kPreviewW,
			kPreviewH);
		drawImage(imgB_, tp.oldOpacity, tp.oldDx, tp.oldDy, tp.oldScale);
		drawImage(imgA_, tp.newOpacity, tp.newDx, tp.newDy, tp.newScale);
	}
	painter.end();

	preview_->setPixmap(QPixmap::fromImage(frame));
}
