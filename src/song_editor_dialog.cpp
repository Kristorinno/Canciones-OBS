#include "song_editor_dialog.h"

#include "letter_size_dialog.h"
#include "store.h"
#include "text_renderer.h"
#include "txt_import.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

SongEditorDialog::SongEditorDialog(const Song &song, bool isNew, QWidget *parent)
	: QDialog(parent), song_(song), theme_(Store::instance().currentTheme())
{
	setWindowTitle(isNew ? QStringLiteral("Editar Canciones - canción nueva")
			     : QStringLiteral("Editar Canciones"));
	resize(1200, 720);

	auto *root = new QVBoxLayout(this);
	auto *body = new QHBoxLayout();
	root->addLayout(body, 1);

	// Izquierda: titulo, artista y letra.
	auto *leftHost = new QWidget(this);
	leftHost->setFixedWidth(420);
	auto *left = new QVBoxLayout(leftHost);
	left->setContentsMargins(0, 0, 0, 0);

	title_ = new QLineEdit(leftHost);
	title_->setPlaceholderText(QStringLiteral("Título"));
	title_->setText(song.title);
	artist_ = new QLineEdit(leftHost);
	artist_->setPlaceholderText(QStringLiteral("Artista o banda"));
	artist_->setText(song.artist);

	lyrics_ = new QPlainTextEdit(leftHost);
	lyrics_->setPlaceholderText(QStringLiteral("Escribe aquí la letra de la canción"));
	QStringList texts;
	for (const Stanza &st : song.stanzas)
		texts << st.text;
	lyrics_->setPlainText(texts.join(QStringLiteral("\n\n")));

	left->addWidget(title_);
	left->addWidget(artist_);
	left->addWidget(lyrics_, 1);
	body->addWidget(leftHost);

	// Derecha: una vista previa por estrofa.
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	cardsHost_ = new QWidget();
	grid_ = new QGridLayout(cardsHost_);
	scroll->setWidget(cardsHost_);
	body->addWidget(scroll, 1);

	auto *buttons = new QDialogButtonBox(this);
	buttons->addButton(QStringLiteral("Guardar"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
	root->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// Las vistas previas se rehacen un instante despues de dejar de escribir.
	timer_ = new QTimer(this);
	timer_->setSingleShot(true);
	timer_->setInterval(300);
	connect(timer_, &QTimer::timeout, this, [this]() {
		rebuildStanzas();
		rebuildCards();
	});
	auto capitalizeWords = [](QLineEdit *edit) {
	QString text = edit->text();
	QString formatted = text;
	bool changed = false;
	for (int i = 0; i < formatted.length(); ++i) {
		if (i == 0 || formatted[i-1].isSpace()) {
			if (formatted[i].isLetter() && formatted[i].isLower()) {
				formatted[i] = formatted[i].toUpper();
				changed = true;
			}
		}
	}
	if (changed) {
		int cursorPos = edit->cursorPosition();
		edit->blockSignals(true);
		edit->setText(formatted);
		edit->setCursorPosition(cursorPos);
		edit->blockSignals(false);
	}
};

connect(title_, &QLineEdit::textEdited, this, [this, capitalizeWords]() { capitalizeWords(title_); });
connect(artist_, &QLineEdit::textEdited, this, [this, capitalizeWords]() { capitalizeWords(artist_); });

connect(lyrics_, &QPlainTextEdit::textChanged, this, [this]() {
	QTextCursor cursor = lyrics_->textCursor();
	int pos = cursor.position();
	QString text = lyrics_->toPlainText();
	QStringList lines = text.split(QLatin1Char('\n'));
	bool changed = false;
	for (int i = 0; i < lines.size(); ++i) {
		if (!lines[i].isEmpty() && lines[i][0].isLower()) {
			lines[i][0] = lines[i][0].toUpper();
			changed = true;
		}
	}
	if (changed) {
		lyrics_->blockSignals(true);
		lyrics_->setPlainText(lines.join(QLatin1Char('\n')));
		cursor.setPosition(pos);
		lyrics_->setTextCursor(cursor);
		lyrics_->blockSignals(false);
	}
	timer_->start();
});

rebuildCards();
}

QPixmap SongEditorDialog::previewFor(int index) const
{
	const Stanza &st = song_.stanzas[index];
	return QPixmap::fromImage(renderStanza(theme_, st.text, st.fontScale, 320,
					       180, QColor(32, 32, 32)));
}

// Vuelve a armar las estrofas desde el texto, conservando el tamanio de letra
// que cada una ya tenia.
void SongEditorDialog::rebuildStanzas()
{
	const QStringList texts = splitStanzas(lyrics_->toPlainText());
	const QVector<Stanza> old = song_.stanzas;
	const bool sameCount = texts.size() == old.size();

	QVector<Stanza> next;
	for (int i = 0; i < texts.size(); ++i) {
		Stanza st = parseStanza(texts[i]);

		if (st.fontScale == -1) {
			st.fontScale = 100;
			if (sameCount) {
				st.fontScale = old[i].fontScale;
			} else {
				for (const Stanza &o : old) {
					if (o.text == st.text) {
						st.fontScale = o.fontScale;
						break;
					}
				}
			}
		}
		next.append(st);
	}
	song_.stanzas = next;
}

void SongEditorDialog::rebuildCards()
{
	previews_.clear();
	while (QLayoutItem *item = grid_->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	const int columns = 2;
	for (int i = 0; i < song_.stanzas.size(); ++i) {
		auto *card = new QFrame(cardsHost_);
		card->setFrameShape(QFrame::StyledPanel);
		auto *box = new QVBoxLayout(card);

		auto *head = new QHBoxLayout();
		head->addWidget(new QLabel(QStringLiteral("Estrofa %1").arg(i), card));
		head->addStretch(1);
		auto *sizeButton =
			new QPushButton(QStringLiteral("Tamaño de letra"), card);
		head->addWidget(sizeButton);
		box->addLayout(head);

		auto *preview = new QLabel(card);
		preview->setPixmap(previewFor(i));
		box->addWidget(preview);
		previews_.append(preview);

		grid_->addWidget(card, i / columns, i % columns);
		connect(sizeButton, &QPushButton::clicked, this,
			[this, i]() { editStanzaSize(i); });
	}
	grid_->setRowStretch(int(song_.stanzas.size()) / columns + 1, 1);
}

void SongEditorDialog::editStanzaSize(int index)
{
	if (index < 0 || index >= song_.stanzas.size())
		return;

	LetterSizeDialog dlg(theme_, song_.stanzas[index].text,
			     song_.stanzas[index].fontScale, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	song_.stanzas[index].fontScale = dlg.scale();
	if (index < previews_.size())
		previews_[index]->setPixmap(previewFor(index));
}

void SongEditorDialog::accept()
{
	const QString title = title_->text().trimmed();
	if (title.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("Falta el título"),
				     QStringLiteral("Escribí el título de la canción."));
		return;
	}

	timer_->stop();
	rebuildStanzas();
	song_.title = title;
	song_.artist = artist_->text().trimmed();
	if (song_.id.isEmpty())
		song_.id = newId();
	QDialog::accept();
}
