#include "text_renderer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QPen>
#include <QStringList>
#include <QVector>

namespace {

struct WrappedLine {
	QString text;
	// true si el parrafo sigue en la linea de abajo (sirve para justificar).
	bool wrapped = false;
};

// Parte el texto en lineas que entren en maxWidth, respetando los saltos
// de linea que ya tenia.
QVector<WrappedLine> wrapText(const QString &text, const QFontMetricsF &fm,
			      double maxWidth)
{
	QVector<WrappedLine> out;
	const QStringList paragraphs = text.split(QLatin1Char('\n'));
	for (const QString &para : paragraphs) {
		const QStringList words =
			para.split(QLatin1Char(' '), Qt::SkipEmptyParts);
		if (words.isEmpty()) {
			out.append(WrappedLine());
			continue;
		}
		QString line;
		for (const QString &word : words) {
			const QString candidate =
				line.isEmpty() ? word
					       : line + QLatin1Char(' ') + word;
			if (line.isEmpty() ||
			    fm.horizontalAdvance(candidate) <= maxWidth) {
				line = candidate;
			} else {
				WrappedLine wl;
				wl.text = line;
				wl.wrapped = true;
				out.append(wl);
				line = word;
			}
		}
		WrappedLine last;
		last.text = line;
		out.append(last);
	}
	return out;
}

// Una pasada de desenfoque (promedio de una ventana) en una direccion.
// Fuera de la imagen se considera transparente.
void blurPass(const QImage &src, QImage &dst, int radius, bool horizontal)
{
	const int len = horizontal ? src.width() : src.height();
	const int lines = horizontal ? src.height() : src.width();
	const int window = radius * 2 + 1;

	for (int l = 0; l < lines; ++l) {
		auto pixelAt = [&](int i) -> QRgb {
			return horizontal
				       ? reinterpret_cast<const QRgb *>(src.constScanLine(l))[i]
				       : reinterpret_cast<const QRgb *>(src.constScanLine(i))[l];
		};

		quint32 a = 0, r = 0, g = 0, b = 0;
		auto add = [&](QRgb p) {
			a += qAlpha(p);
			r += qRed(p);
			g += qGreen(p);
			b += qBlue(p);
		};
		auto sub = [&](QRgb p) {
			a -= qAlpha(p);
			r -= qRed(p);
			g -= qGreen(p);
			b -= qBlue(p);
		};

		for (int i = 0; i < qMin(radius + 1, len); ++i)
			add(pixelAt(i));

		for (int i = 0; i < len; ++i) {
			const QRgb out = qRgba(int(r / window), int(g / window),
					       int(b / window), int(a / window));
			if (horizontal)
				reinterpret_cast<QRgb *>(dst.scanLine(l))[i] = out;
			else
				reinterpret_cast<QRgb *>(dst.scanLine(i))[l] = out;

			const int addIdx = i + radius + 1;
			if (addIdx < len)
				add(pixelAt(addIdx));
			const int subIdx = i - radius;
			if (subIdx >= 0)
				sub(pixelAt(subIdx));
		}
	}
}

// Desenfoque aproximado (dos pasadas de cada direccion).
void boxBlur(QImage &img, int radius)
{
	if (radius < 1 || img.isNull())
		return;
	QImage tmp(img.size(), QImage::Format_ARGB32_Premultiplied);
	for (int pass = 0; pass < 2; ++pass) {
		blurPass(img, tmp, radius, true);
		blurPass(tmp, img, radius, false);
	}
}

// Desenfoca trabajando en una copia mas chica, para que sea rapido.
void blurScaled(QImage &img, double radiusPx)
{
	if (radiusPx < 1.0 || img.isNull())
		return;

	int factor = 1;
	if (radiusPx >= 12.0)
		factor = 4;
	else if (radiusPx >= 4.0)
		factor = 2;

	if (factor == 1) {
		boxBlur(img, int(radiusPx + 0.5));
		return;
	}

	const QSize full = img.size();
	QImage small = img.scaled(qMax(1, full.width() / factor),
				  qMax(1, full.height() / factor),
				  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	small = small.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	boxBlur(small, qMax(1, int(radiusPx / factor + 0.5)));
	img = small.scaled(full, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
		      .convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// Copia de la imagen pintada toda de un color (conserva la forma).
QImage tinted(const QImage &src, const QColor &color)
{
	QImage out = src;
	QPainter p(&out);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(out.rect(), color);
	p.end();
	return out;
}

struct LineLayout {
	QString text;
	double x = 0.0;
	double width = 0.0;
	double top = 0.0;
	double baseline = 0.0;
	// Pedazos de texto con su posicion en x (varios si esta justificada).
	QVector<QPair<double, QString>> segments;
};

} // namespace

QRectF textArea(const Theme &t, int w, int h)
{
	const double left = w * t.marginLeft / 100.0 + w * t.offsetX / 100.0;
	const double top = h * t.marginTop / 100.0 - h * t.offsetY / 100.0;
	double width = w * (100 - t.marginLeft - t.marginRight) / 100.0;
	double height = h * (100 - t.marginTop - t.marginBottom) / 100.0;
	if (width < 10.0)
		width = 10.0;
	if (height < 10.0)
		height = 10.0;
	return QRectF(left, top, width, height);
}

QImage renderStanza(const Theme &theme, const QString &text,
		    int fontScalePercent, int width, int height,
		    const QColor &background)
{
	QImage img(qMax(1, width), qMax(1, height),
		   QImage::Format_ARGB32_Premultiplied);
	img.fill(background);

	QString clean = text;
	clean.replace(QLatin1Char('\r'), QString());
	clean = clean.trimmed();
	if (theme.uppercase)
		clean = clean.toUpper();
	if (clean.isEmpty())
		return img;

	const double scale = img.height() / 1080.0;
	const QRectF area = textArea(theme, img.width(), img.height());

	QFont font(theme.fontFamily);
	font.setBold(theme.bold);
	font.setItalic(theme.italic);

	// Si el texto no entra en el area, se achica hasta que entre.
	double px = img.height() * theme.fontSize / 100.0 * fontScalePercent / 100.0;
	QVector<WrappedLine> lines;
	double lineHeight = 0.0;
	QFontMetricsF metrics(font);
	for (int attempt = 0; attempt < 40; ++attempt) {
		font.setPixelSize(qMax(6, int(px + 0.5)));
		font.setLetterSpacing(QFont::AbsoluteSpacing,
				      theme.charSpacing * scale);
		metrics = QFontMetricsF(font);
		lines = wrapText(clean, metrics, area.width());
		lineHeight = metrics.lineSpacing() * theme.lineSpacing / 100.0;
		const double total = lineHeight * lines.size();
		if (total <= area.height() || px <= 8.0)
			break;
		px *= 0.93;
	}

	const double totalHeight = lineHeight * lines.size();
	double y0 = area.top();
	if (theme.vAlign == 1)
		y0 = area.top() + (area.height() - totalHeight) / 2.0;
	else if (theme.vAlign == 2)
		y0 = area.bottom() - totalHeight;

	// Posicion de cada linea.
	QVector<LineLayout> layout;
	for (int i = 0; i < lines.size(); ++i) {
		const WrappedLine &wl = lines[i];
		LineLayout L;
		L.text = wl.text;
		L.top = y0 + i * lineHeight;
		L.baseline = L.top + metrics.ascent();
		L.width = metrics.horizontalAdvance(wl.text);

		L.x = area.left();
		if (theme.hAlign == 1)
			L.x = area.left() + (area.width() - L.width) / 2.0;
		else if (theme.hAlign == 2)
			L.x = area.right() - L.width;

		const QStringList words =
			wl.text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
		if (theme.hAlign == 3 && wl.wrapped && words.size() > 1) {
			// Justificado: se reparte el espacio sobrante entre las palabras.
			double wordsWidth = 0.0;
			for (const QString &word : words)
				wordsWidth += metrics.horizontalAdvance(word);
			const double gap =
				(area.width() - wordsWidth) / (words.size() - 1);
			double x = area.left();
			for (const QString &word : words) {
				L.segments.append(qMakePair(x, word));
				x += metrics.horizontalAdvance(word) + gap;
			}
			L.x = area.left();
			L.width = area.width();
		} else {
			L.segments.append(qMakePair(L.x, wl.text));
		}
		layout.append(L);
	}

	// Rectangulos para la caja de fondo y el borde.
	auto lineRects = [&](double padX, double padY, int type, int widthMode) {
		QVector<QRectF> rects;
		QRectF all;
		bool first = true;
		for (const LineLayout &L : layout) {
			if (L.text.isEmpty())
				continue;
			// Usamos la altura real de la letra (ascent + descent) en vez de lineHeight.
			// Esto permite que padY agrande la caja sin que se pegue con las lineas de al lado.
			const double textTop = L.baseline - metrics.ascent();
			const double textHeight = metrics.ascent() + metrics.descent();
			
			double boxX = L.x;
			double boxWidth = L.width;
			if (widthMode == 1) {
				boxX = area.left();
				boxWidth = area.width();
			}
			
			const QRectF r(boxX, textTop, boxWidth, textHeight);

			if (type == 0) {
				// Limitamos el padY para que quede un pequeno espacio (gap) entre cajas
				const double gap = 2.0 * scale;
				const double maxPadY = qMax(0.0, (lineHeight - textHeight - gap) / 2.0);
				const double actualPadY = qMin(padY, maxPadY);
				rects.append(r.adjusted(-padX, -actualPadY, padX, actualPadY));
			} else {
				all = first ? r : all.united(r);
				first = false;
			}
		}
		if (type != 0 && !first)
			rects.append(all.adjusted(-padX, -padY, padX, padY));
		return rects;
	};

	// 1. Caja de fondo y borde (debajo del texto)
	{
		QPainter painter(&img);
		painter.setRenderHint(QPainter::Antialiasing, true);

		if (theme.bgEnabled) {
			QColor c = theme.bgColor;
			c.setAlpha(qBound(0, theme.bgOpacity, 100) * 255 / 100);
			painter.setPen(Qt::NoPen);
			painter.setBrush(c);
			const double radius = theme.bgRadius * scale;
			for (const QRectF &r :
			     lineRects(theme.bgPadX * scale, theme.bgPadY * scale,
				       theme.bgType, 0))
				painter.drawRoundedRect(r, radius, radius);
		}

		if (theme.boxEnabled) {
			QColor fc = theme.boxFillColor;
			fc.setAlpha(qBound(0, theme.boxFillOpacity, 100) * 255 / 100);
			painter.setBrush(fc); // Ahora rellena con el color
			
			// Usamos un lápiz del mismo color con grosor mínimo para suavizar los bordes (antialiasing)
			painter.setPen(QPen(fc, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
			const double radius = theme.boxRadius * scale;
			for (const QRectF &r :
			     lineRects(theme.boxPadX * scale, theme.boxPadY * scale,
				       theme.boxType, theme.boxWidthMode))
				painter.drawRoundedRect(r, radius, radius);
		}
	}

	// 2. El texto solo, en una capa transparente
	QImage textLayer(img.size(), QImage::Format_ARGB32_Premultiplied);
	textLayer.fill(Qt::transparent);
	{
		QPainter p(&textLayer);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setRenderHint(QPainter::TextAntialiasing, true);
		p.setFont(font);

		const double outlineWidth = theme.outlineWidth * scale;
		for (const LineLayout &L : layout) {
			for (const QPair<double, QString> &seg : L.segments) {
				if (seg.second.isEmpty())
					continue;
				if (theme.outline && outlineWidth > 0.0) {
					QPainterPath path;
					path.addText(QPointF(seg.first, L.baseline), font,
						     seg.second);
					p.setPen(QPen(theme.outlineColor,
						      outlineWidth * 2.0, Qt::SolidLine,
						      Qt::RoundCap, Qt::RoundJoin));
					p.setBrush(Qt::NoBrush);
					p.drawPath(path);
					p.setPen(Qt::NoPen);
					p.setBrush(theme.color);
					p.drawPath(path);
					p.setBrush(Qt::NoBrush);
				} else {
					p.setPen(theme.color);
					p.drawText(QPointF(seg.first, L.baseline),
						   seg.second);
				}
			}
		}
	}

	// 3. Brillo, sombra y texto, en ese orden
	{
		QPainter painter(&img);

		if (theme.glow && theme.glowSize > 0) {
			QImage glow = tinted(textLayer, theme.glowColor);
			blurScaled(glow, theme.glowSize * scale);
			painter.drawImage(0, 0, glow);
			painter.drawImage(0, 0, glow);
		}

		if (theme.shadow) {
			QImage shadow = tinted(textLayer, theme.shadowColor);
			if (theme.shadowBlur)
				blurScaled(shadow,
					   qMax(2.0, theme.shadowDistance * scale * 1.5));
			const int dir = qBound(0, theme.shadowDir, 8);
			const double dx = ((dir % 3) - 1) * theme.shadowDistance * scale;
			const double dy = ((dir / 3) - 1) * theme.shadowDistance * scale;
			painter.drawImage(QPointF(dx, dy), shadow);
		}

		if (theme.textTransparent) {
			// Esto "perfora" la imagen para que se vea el fondo de OBS
			painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
			painter.drawImage(0, 0, textLayer);
			painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		} else {
			painter.drawImage(0, 0, textLayer);
		}
	}
	return img;
}