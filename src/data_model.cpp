#include "data_model.h"

#include <QJsonArray>
#include <QUuid>

QString newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString foldText(const QString &s)
{
	const QString decomposed = s.normalized(QString::NormalizationForm_D);
	QString out;
	out.reserve(decomposed.size());
	for (const QChar &c : decomposed) {
		if (c.category() != QChar::Mark_NonSpacing)
			out.append(c);
	}
	return out.toLower();
}

QString Song::allText() const
{
	QStringList parts;
	for (const Stanza &st : stanzas)
		parts << st.text;
	return parts.join(QLatin1Char('\n'));
}

Theme defaultTheme()
{
	Theme t;
	t.id = newId();
	t.name = QStringLiteral("Tema básico");
	return t;
}

namespace {

QString colorToString(const QColor &c)
{
	return c.name(QColor::HexArgb);
}

QColor colorFromJson(const QJsonObject &o, const QString &key, const QColor &def)
{
	const QColor c(o.value(key).toString());
	return c.isValid() ? c : def;
}

} // namespace

QJsonObject songToJson(const Song &song)
{
	QJsonObject o;
	o.insert(QStringLiteral("id"), song.id);
	o.insert(QStringLiteral("title"), song.title);
	o.insert(QStringLiteral("artist"), song.artist);
	o.insert(QStringLiteral("defaultCardFontSize"), song.defaultCardFontSize);

	QJsonArray arr;
	for (const Stanza &st : song.stanzas) {
		QJsonObject so;
		so.insert(QStringLiteral("text"), st.text);
		so.insert(QStringLiteral("scale"), st.fontScale);
		so.insert(QStringLiteral("cardFontSize"), st.cardFontSize);
		arr.append(so);
	}
	o.insert(QStringLiteral("stanzas"), arr);
	return o;
}

Song songFromJson(const QJsonObject &obj)
{
	Song s;
	s.id = obj.value(QStringLiteral("id")).toString();
	s.title = obj.value(QStringLiteral("title")).toString();
	s.artist = obj.value(QStringLiteral("artist")).toString();
	s.defaultCardFontSize = obj.value(QStringLiteral("defaultCardFontSize")).toInt(-1);

	const QJsonArray arr = obj.value(QStringLiteral("stanzas")).toArray();
	for (const QJsonValue &v : arr) {
		const QJsonObject so = v.toObject();
		Stanza st;
		st.text = so.value(QStringLiteral("text")).toString();
		st.fontScale = so.value(QStringLiteral("scale")).toInt(100);
		st.cardFontSize = so.value(QStringLiteral("cardFontSize")).toInt(-1);
		s.stanzas.append(st);
	}
	return s;
}

QJsonObject themeToJson(const Theme &t)
{
	QJsonObject o;
	o.insert(QStringLiteral("id"), t.id);
	o.insert(QStringLiteral("name"), t.name);

	o.insert(QStringLiteral("fontFamily"), t.fontFamily);
	o.insert(QStringLiteral("fontSizePct"), t.fontSize);
	o.insert(QStringLiteral("color"), colorToString(t.color));
	o.insert(QStringLiteral("bold"), t.bold);
	o.insert(QStringLiteral("italic"), t.italic);
	o.insert(QStringLiteral("uppercase"), t.uppercase);
	o.insert(QStringLiteral("charSpacing"), t.charSpacing);
	o.insert(QStringLiteral("lineSpacing"), t.lineSpacing);
	o.insert(QStringLiteral("hAlign"), t.hAlign);
	o.insert(QStringLiteral("vAlign"), t.vAlign);

	o.insert(QStringLiteral("marginTop"), t.marginTop);
	o.insert(QStringLiteral("marginBottom"), t.marginBottom);
	o.insert(QStringLiteral("marginLeft"), t.marginLeft);
	o.insert(QStringLiteral("marginRight"), t.marginRight);
	o.insert(QStringLiteral("offsetX"), t.offsetX);
	o.insert(QStringLiteral("offsetY"), t.offsetY);

	o.insert(QStringLiteral("outline"), t.outline);
	o.insert(QStringLiteral("outlineWidth"), t.outlineWidth);
	o.insert(QStringLiteral("outlineColor"), colorToString(t.outlineColor));

	o.insert(QStringLiteral("glow"), t.glow);
	o.insert(QStringLiteral("glowColor"), colorToString(t.glowColor));
	o.insert(QStringLiteral("glowSize"), t.glowSize);

	o.insert(QStringLiteral("shadow"), t.shadow);
	o.insert(QStringLiteral("shadowColor"), colorToString(t.shadowColor));
	o.insert(QStringLiteral("shadowDir"), t.shadowDir);
	o.insert(QStringLiteral("shadowDistance"), t.shadowDistance);
	o.insert(QStringLiteral("shadowBlur"), t.shadowBlur);

	o.insert(QStringLiteral("bgEnabled"), t.bgEnabled);
	o.insert(QStringLiteral("bgColor"), colorToString(t.bgColor));
	o.insert(QStringLiteral("bgOpacity"), t.bgOpacity);
	o.insert(QStringLiteral("bgPadX"), t.bgPadX);
	o.insert(QStringLiteral("bgPadY"), t.bgPadY);
	o.insert(QStringLiteral("bgRadius"), t.bgRadius);
	o.insert(QStringLiteral("bgType"), t.bgType);

	o.insert(QStringLiteral("boxEnabled"), t.boxEnabled);
	o.insert(QStringLiteral("boxFillColor"), colorToString(t.boxFillColor));
	o.insert(QStringLiteral("boxFillOpacity"), t.boxFillOpacity);
	o.insert(QStringLiteral("textTransparent"), t.textTransparent);
	o.insert(QStringLiteral("boxPadX"), t.boxPadX);
	o.insert(QStringLiteral("boxPadY"), t.boxPadY);
	o.insert(QStringLiteral("boxRadius"), t.boxRadius);
	o.insert(QStringLiteral("boxType"), t.boxType);
	o.insert(QStringLiteral("boxWidthMode"), t.boxWidthMode);
	return o;
}

Theme themeFromJson(const QJsonObject &o)
{
	Theme t;
	t.id = o.value(QStringLiteral("id")).toString();
	if (t.id.isEmpty())
		t.id = newId();
	t.name = o.value(QStringLiteral("name")).toString(t.name);

	t.fontFamily = o.value(QStringLiteral("fontFamily")).toString(t.fontFamily);
	t.fontSize = o.value(QStringLiteral("fontSizePct")).toDouble(t.fontSize);
	t.color = colorFromJson(o, QStringLiteral("color"), t.color);
	t.bold = o.value(QStringLiteral("bold")).toBool(t.bold);
	t.italic = o.value(QStringLiteral("italic")).toBool(t.italic);
	t.uppercase = o.value(QStringLiteral("uppercase")).toBool(t.uppercase);
	t.charSpacing = o.value(QStringLiteral("charSpacing")).toInt(t.charSpacing);
	t.lineSpacing = o.value(QStringLiteral("lineSpacing")).toInt(t.lineSpacing);
	t.hAlign = o.value(QStringLiteral("hAlign")).toInt(t.hAlign);
	t.vAlign = o.value(QStringLiteral("vAlign")).toInt(t.vAlign);

	t.marginTop = o.value(QStringLiteral("marginTop")).toInt(t.marginTop);
	t.marginBottom = o.value(QStringLiteral("marginBottom")).toInt(t.marginBottom);
	t.marginLeft = o.value(QStringLiteral("marginLeft")).toInt(t.marginLeft);
	t.marginRight = o.value(QStringLiteral("marginRight")).toInt(t.marginRight);
	t.offsetX = o.value(QStringLiteral("offsetX")).toInt(t.offsetX);
	t.offsetY = o.value(QStringLiteral("offsetY")).toInt(t.offsetY);

	t.outline = o.value(QStringLiteral("outline")).toBool(t.outline);
	t.outlineWidth = o.value(QStringLiteral("outlineWidth")).toInt(t.outlineWidth);
	t.outlineColor = colorFromJson(o, QStringLiteral("outlineColor"), t.outlineColor);

	t.glow = o.value(QStringLiteral("glow")).toBool(t.glow);
	t.glowColor = colorFromJson(o, QStringLiteral("glowColor"), t.glowColor);
	t.glowSize = o.value(QStringLiteral("glowSize")).toInt(t.glowSize);

	t.shadow = o.value(QStringLiteral("shadow")).toBool(t.shadow);
	t.shadowColor = colorFromJson(o, QStringLiteral("shadowColor"), t.shadowColor);
	t.shadowDir = o.value(QStringLiteral("shadowDir")).toInt(t.shadowDir);
	t.shadowDistance = o.value(QStringLiteral("shadowDistance")).toInt(t.shadowDistance);
	t.shadowBlur = o.value(QStringLiteral("shadowBlur")).toBool(t.shadowBlur);

	t.bgEnabled = o.value(QStringLiteral("bgEnabled")).toBool(t.bgEnabled);
	t.bgColor = colorFromJson(o, QStringLiteral("bgColor"), t.bgColor);
	t.bgOpacity = o.value(QStringLiteral("bgOpacity")).toInt(t.bgOpacity);
	t.bgPadX = o.value(QStringLiteral("bgPadX")).toInt(t.bgPadX);
	t.bgPadY = o.value(QStringLiteral("bgPadY")).toInt(t.bgPadY);
	t.bgRadius = o.value(QStringLiteral("bgRadius")).toInt(t.bgRadius);
	t.bgType = o.value(QStringLiteral("bgType")).toInt(t.bgType);

	t.boxEnabled = o.value(QStringLiteral("boxEnabled")).toBool(t.boxEnabled);
	t.boxFillColor = colorFromJson(o, QStringLiteral("boxFillColor"), t.boxFillColor);
	t.boxFillOpacity = o.value(QStringLiteral("boxFillOpacity")).toInt(t.boxFillOpacity);
	t.textTransparent = o.value(QStringLiteral("textTransparent")).toBool(t.textTransparent);
	t.boxPadX = o.value(QStringLiteral("boxPadX")).toInt(t.boxPadX);
	t.boxPadY = o.value(QStringLiteral("boxPadY")).toInt(t.boxPadY);
	t.boxRadius = o.value(QStringLiteral("boxRadius")).toInt(t.boxRadius);
	t.boxType = o.value(QStringLiteral("boxType")).toInt(t.boxType);
	t.boxWidthMode = o.value(QStringLiteral("boxWidthMode")).toInt(t.boxWidthMode);
	return t;
}