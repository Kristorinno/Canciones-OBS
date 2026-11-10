#include "main_window.h"
#include "data_model.h"
#include "letras_state.h"
#include "song_editor_dialog.h"
#include "store.h"
#include "text_renderer.h"
#include "theme_editor_dialog.h"
#include "transitions_dialog.h"
#include "txt_import.h"
#include "web_server.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QClipboard>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QRegularExpression>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <obs-frontend-api.h>
#include <QImage>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

const int kIdRole = Qt::UserRole;
const int kArtistRole = Qt::UserRole + 1;

const QColor kActiveColor(42, 67, 101);

// Dibuja cada cancion de la lista de reproduccion con el titulo y, justo
// abajo, el artista en letra mas chica.
class PlaylistDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);
		const QString title = index.data(Qt::DisplayRole).toString();
		const QString artist = index.data(kArtistRole).toString();
		opt.text.clear();

		QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

		// Margen para que no toque la barra de scroll
		const QRect r = opt.rect.adjusted(8, 4, -8, -4);
		const QColor textColor = opt.palette.color(
			opt.state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText
								    : QPalette::Text);

		painter->save();
		painter->setPen(textColor);

		QFont titleFont = opt.font;
		titleFont.setBold(true);
		
		QFont artistFont = opt.font;
		if (opt.font.pointSizeF() > 0)
			artistFont.setPointSizeF(opt.font.pointSizeF() * 0.85);

		QFontMetrics fmTitle(titleFont);
		QFontMetrics fmArtist(artistFont);

		// Calculamos cuánto mide el texto real para centrarlo
		QRect titleBound = fmTitle.boundingRect(QRect(0, 0, r.width(), 1000), Qt::TextWordWrap | Qt::AlignHCenter, title);
		QRect artistBound;
		if (!artist.isEmpty()) {
			artistBound = fmArtist.boundingRect(QRect(0, 0, r.width(), 1000), Qt::TextWordWrap | Qt::AlignHCenter, artist);
		}

		int totalHeight = titleBound.height();
		if (!artist.isEmpty()) {
			totalHeight += artistBound.height() + 4; // Separación entre título y autor
		}

		// Lo centramos verticalmente en el espacio disponible
		int startY = r.top() + (r.height() - totalHeight) / 2;
		if (startY < r.top()) startY = r.top();

		QRect titleRect(r.left(), startY, r.width(), titleBound.height());
		painter->setFont(titleFont);
		painter->drawText(titleRect, int(Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap), title);

		if (!artist.isEmpty()) {
			QRect artistRect(r.left(), titleRect.bottom() + 4, r.width(), artistBound.height());
			painter->setFont(artistFont);
			painter->drawText(artistRect, int(Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap), artist);
		}

		painter->restore();
	}

	QSize sizeHint(const QStyleOptionViewItem &option,
		       const QModelIndex &index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);
		const QString title = index.data(Qt::DisplayRole).toString();
		const QString artist = index.data(kArtistRole).toString();

		QFont titleFont = opt.font;
		titleFont.setBold(true);
		QFont artistFont = opt.font;
		if (opt.font.pointSizeF() > 0)
			artistFont.setPointSizeF(opt.font.pointSizeF() * 0.85);

		QFontMetrics fmTitle(titleFont);
		QFontMetrics fmArtist(artistFont);

		// Acá calculamos el ancho REAL de la columna restándole la barra de scroll (20px)
		int availableWidth = (option.widget ? option.widget->width() : 200) - 20;
		if (availableWidth < 50) availableWidth = 50;

		// Calculamos la altura necesaria para el texto
		int height = fmTitle.boundingRect(QRect(0, 0, availableWidth, 1000), Qt::TextWordWrap, title).height();
		if (!artist.isEmpty()) {
			height += fmArtist.boundingRect(QRect(0, 0, availableWidth, 1000), Qt::TextWordWrap, artist).height() + 4;
		}

		// Altura mínima de 40px para que no quede apretado, más 16px de márgenes
		height = qMax(40, height + 16); 

		return QSize(availableWidth, height);
	}
};

// Delegado para resaltar las palabras que coinciden con la búsqueda.
static const QRegularExpression &lineBreakRegex()
{
	// Cubre \n, \r\n, \r, separador de línea/párrafo Unicode y tabulación vertical
	static const QRegularExpression re(QStringLiteral("\\r\\n|[\\r\\n\\x{2028}\\x{2029}\\x{000B}]"));
	return re;
}

static QString stanzaTextToHtml(const QString &text)
{
	QStringList parts = text.split(lineBreakRegex());
	for (QString &part : parts)
		part = part.toHtmlEscaped();
	return parts.join(QStringLiteral("<br>"));
}

class HighlightDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	QString query;

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override
	{
		QStyleOptionViewItem opt = option;
initStyleOption(&opt, index);

const QString text = opt.text;
opt.text.clear();

		QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

		if (text.isEmpty()) return;

		QTextDocument doc;
doc.setDefaultFont(opt.font);
if (query.trimmed().isEmpty()) {
	doc.setHtml(stanzaTextToHtml(text));
} else {
	doc.setHtml(highlightText(text, query.trimmed()));
}

		QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

int availableWidth = textRect.width();
if (availableWidth <= 0 && option.widget) {
	availableWidth = option.widget->width() - 20; // 20px para el scroll
}
if (availableWidth < 50) availableWidth = 50;

doc.setTextWidth(availableWidth);

		painter->save();
		painter->translate(textRect.topLeft());

		QAbstractTextDocumentLayout::PaintContext ctx;
		ctx.palette.setColor(QPalette::Text, opt.palette.color(
			opt.state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
		doc.documentLayout()->draw(painter, ctx);
		painter->restore();
	}

	QSize sizeHint(const QStyleOptionViewItem &option,
		       const QModelIndex &index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);
		const QString text = opt.text;

		QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

int availableWidth = textRect.width();
if (availableWidth <= 0 && option.widget) {
	availableWidth = option.widget->width() - 20;
}
if (availableWidth < 50) availableWidth = 50;

QTextDocument doc;
doc.setDefaultFont(opt.font);
if (query.trimmed().isEmpty()) {
	doc.setHtml(stanzaTextToHtml(text));
} else {
	doc.setHtml(highlightText(text, query.trimmed()));
}
doc.setTextWidth(availableWidth);

int h = qCeil(doc.size().height());
return QSize(availableWidth, qMax(30, h + 16));
	}

private:
static QString highlightText(const QString &text, const QString &query)
{
	QString foldedQuery = foldText(query);
	QStringList lines = text.split(lineBreakRegex());
	QStringList processedLines;

	for (const QString &line : lines) {
		QString htmlLine;
		int lastPos = 0;
		int i = 0;
		int n = line.length();

		while (i < n) {
			int start = i;
			while (i < n && !line[i].isSpace()) i++;
			if (i > start) {
				QString word = line.mid(start, i - start);
				if (start > lastPos) {
					htmlLine += line.mid(lastPos, start - lastPos).toHtmlEscaped();
				}
				if (foldText(word).startsWith(foldedQuery)) {
					int matchLen = 0;
					int currentFoldedLen = 0;
					int originalIdx = 0;
					while (originalIdx < word.length() && currentFoldedLen < foldedQuery.length()) {
						QString sub = word.mid(originalIdx, 1);
						currentFoldedLen += foldText(sub).length();
						originalIdx++;
					}
					matchLen = originalIdx;

					QString prefix = word.left(matchLen);
					QString rest = word.mid(matchLen);
					htmlLine += QStringLiteral("<span style=\"background-color: rgba(100, 150, 255, 100); border-radius: 3px;\">%1</span>%2")
							.arg(prefix.toHtmlEscaped(), rest.toHtmlEscaped());
				} else {
					htmlLine += word.toHtmlEscaped();
				}
				lastPos = i;
			}
			i++;
		}
		if (lastPos < n) {
			htmlLine += line.mid(lastPos).toHtmlEscaped();
		}
		processedLines.append(htmlLine);
	}
			return processedLines.join(QStringLiteral("<br>"));
	}
};
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	themeThumbWidth_ = Store::instance().themeThumbWidth;
	buildUi();
	rebuildSearchCache();
	refreshSongList();
	refreshPlaylist();
	refreshThemes();

	// Atajos de teclado (Flechas, F9 y números 0 a 9)
	auto *scLeft = new QShortcut(QKeySequence(Qt::Key_Left), this);
	connect(scLeft, &QShortcut::activated, this, &MainWindow::previousStanza);

	auto *scRight = new QShortcut(QKeySequence(Qt::Key_Right), this);
	connect(scRight, &QShortcut::activated, this, &MainWindow::nextStanza);

	auto *scF9 = new QShortcut(QKeySequence(Qt::Key_F9), this);
scF9->setContext(Qt::WindowShortcut); // Solo funciona con la ventana activa
connect(scF9, &QShortcut::activated, this, &MainWindow::toggleHidden);

	for (int i = 0; i <= 9; ++i) {
		auto *sc = new QShortcut(QKeySequence(Qt::Key_0 + i), this);
		connect(sc, &QShortcut::activated, this, [this, i]() {
			projectStanza(i);
		});
	}
}

MainWindow::~MainWindow()
{
	if (isVisible()) {
		Store::instance().windowGeometry = saveGeometry();
		Store::instance().splitterState = splitter_->saveState();
		Store::instance().themeThumbWidth = themeThumbWidth_;
		Store::instance().saveSettings();
	}
}

void MainWindow::openWindow()
{
	if (isMinimized()) {
		setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	} else if (!isVisible()) {
		const QByteArray geometry = Store::instance().windowGeometry;
		if (!geometry.isEmpty()) {
			restoreGeometry(geometry);
			show();
		} else {
			showMaximized();
		}
	}
	raise();
	activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	Store::instance().windowGeometry = saveGeometry();
	Store::instance().splitterState = splitter_->saveState();
	Store::instance().themeThumbWidth = themeThumbWidth_;
	Store::instance().saveSettings();
	hide();
	event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	updateMonitor();
}

void MainWindow::buildUi()
{
	setWindowTitle(QStringLiteral("Canciones OBS"));
	resize(1280, 720);

	// Barra de herramientas superior
	QToolBar *bar = addToolBar(QStringLiteral("Herramientas"));
	bar->setMovable(false);
	QAction *addSongAction = bar->addAction(QStringLiteral("Agregar canciones"));
	QAction *transitionsAction = bar->addAction(QStringLiteral("Transiciones"));
	QAction *importAction =
		bar->addAction(QStringLiteral("Importar canciones (.txt)"));
	QAction *qrAction =
		bar->addAction(QStringLiteral("Conectar celular web"));
	connect(addSongAction, &QAction::triggered, this, [this]() { addSong(); });
	connect(transitionsAction, &QAction::triggered, this,
		[this]() { openTransitions(); });
	connect(importAction, &QAction::triggered, this, [this]() { importTxt(); });
	connect(qrAction, &QAction::triggered, this, &MainWindow::showQrCode);

	auto *central = new QWidget(this);
	auto *rootLayout = new QVBoxLayout(central);
	splitter_ = new QSplitter(Qt::Horizontal, central);
rootLayout->addWidget(splitter_, 1);

	// 1. Canciones cargadas
	auto *songsPanel = new QWidget();
songsPanel->setMinimumWidth(220); // Límite menor para Canciones
auto *songsLayout = new QVBoxLayout(songsPanel);
	songsLayout->addWidget(new QLabel(QStringLiteral("Canciones cargadas"), songsPanel));
	songSearch_ = new QLineEdit(songsPanel);
	songSearch_->setPlaceholderText(QStringLiteral("Buscar en títulos y letras..."));
	songSearch_->setClearButtonEnabled(true);
	songsLayout->addWidget(songSearch_);
	songList_ = new QListWidget(songsPanel);
songList_->setItemDelegate(new HighlightDelegate(songList_));
songList_->setContextMenuPolicy(Qt::CustomContextMenu);
	songList_->setStyleSheet(QStringLiteral(
		"QListWidget::item { "
		"  background-color: #2d2d2d; "
		"  border: 1px solid #3a3a3a; "
		"  border-radius: 4px; "
		"  margin: 3px; "
		"  padding: 4px; "
			"} "
	"QListWidget::item:selected { "
	"  background-color: #2a4365; "
	"  border: 1px solid #3b5d91; "
	"  color: white; "
	"}"
));
songsLayout->addWidget(songList_, 1);
	splitter_->addWidget(songsPanel);

	// 2. Vista previa de la cancion seleccionada
auto *centerPanel = new QWidget();
centerPanel->setMinimumWidth(250); // Límite menor para la Letra
auto *centerLayout = new QVBoxLayout(centerPanel);
	songLabel_ = new QLabel(QStringLiteral("Elegí una canción de la lista"), centerPanel);
	songLabel_->setStyleSheet(QStringLiteral("font-weight: bold;"));
	centerLayout->addWidget(songLabel_);

	stanzaList_ = new QListWidget(centerPanel);
stanzaList_->setItemDelegate(new HighlightDelegate(stanzaList_));
stanzaList_->setWordWrap(true);
	stanzaList_->setSelectionMode(QAbstractItemView::NoSelection);
	stanzaList_->setStyleSheet(QStringLiteral(
		"QListWidget::item { "
		"  border-bottom: 1px solid #444444; "
		"  padding: 8px 4px; "
			"} "
	"QListWidget::item:selected { "
	"  background-color: #2a4365; "
	"}"
));
centerLayout->addWidget(stanzaList_, 1);
	splitter_->addWidget(centerPanel);

// 3. Temas
auto *themesPanel = new QWidget();
auto *themesLayout = new QVBoxLayout(themesPanel);
auto *themesHead = new QHBoxLayout();
themesHead->addWidget(new QLabel(QStringLiteral("Temas"), themesPanel));
themesHead->addStretch(1);
auto *newThemeButton = new QPushButton(QStringLiteral("+"), themesPanel);
newThemeButton->setFixedWidth(36);
newThemeButton->setToolTip(QStringLiteral("Crear un tema nuevo"));
themesHead->addWidget(newThemeButton);
themesLayout->addLayout(themesHead);

monitorLabel_ = new QLabel(themesPanel);
monitorLabel_->setMinimumSize(100, 56);
monitorLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
monitorLabel_->setStyleSheet(QStringLiteral("background-color: #202020; border: 1px solid #444;"));
themesLayout->addWidget(monitorLabel_);
themeList_ = new QListWidget(themesPanel);
themeList_->setViewMode(QListView::IconMode);
themeList_->setStyleSheet(QStringLiteral(
	"QListWidget::item { "
	"  background-color: #2d2d2d; "
	"  border: 1px solid #3a3a3a; "
	"  border-radius: 4px; "
	"  margin: 4px; "
	"  padding: 4px; "
	"}"
));
themeList_->setIconSize(QSize(themeThumbWidth_, themeThumbWidth_ * 9 / 16));
themeList_->setGridSize(QSize(themeThumbWidth_ + 20, themeThumbWidth_ * 9 / 16 + 40));
themeList_->setResizeMode(QListView::Adjust);
themeList_->setMovement(QListView::Static);
themeList_->setSpacing(6);
themeList_->setSelectionMode(QAbstractItemView::NoSelection);
themeList_->setContextMenuPolicy(Qt::CustomContextMenu);

// Evita que la lista se desborde a la derecha
themeList_->setFlow(QListView::LeftToRight);
themeList_->setWrapping(true);
themeList_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
themeList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

// Límite menor para la columna de Temas
themesPanel->setMinimumWidth(themeThumbWidth_ + 40); 

themesLayout->addWidget(themeList_, 1);

splitter_->addWidget(themesPanel);

// 4. Lista de reproduccion
auto *playlistPanel = new QWidget();
playlistPanel->setMinimumWidth(220); // Límite menor para la Lista
auto *playlistLayout = new QVBoxLayout(playlistPanel);
playlistLayout->addWidget(
	new QLabel(QStringLiteral("Lista de reproducción"), playlistPanel));
playlist_ = new QListWidget(playlistPanel);
playlist_->setItemDelegate(new PlaylistDelegate(playlist_));
playlist_->setContextMenuPolicy(Qt::CustomContextMenu);
playlist_->setWordWrap(true);
playlist_->setResizeMode(QListView::Adjust);
playlist_->setStyleSheet(QStringLiteral(
	"QListWidget::item { "
	"  background-color: #2d2d2d; "
	"  border: 1px solid #3a3a3a; "
	"  border-radius: 4px; "
	"  margin: 4px; "
	"} "
	"QListWidget::item:selected { "
	"  background-color: #2a4365; "
	"  border: 1px solid #3b5d91; "
	"}"
));
playlistLayout->addWidget(playlist_, 1);
auto *clearButton = new QPushButton(QStringLiteral("Limpiar lista"), playlistPanel);
playlistLayout->addWidget(clearButton);
splitter_->addWidget(playlistPanel);

if (!Store::instance().splitterState.isEmpty()) {
	splitter_->restoreState(Store::instance().splitterState);
} else {
	splitter_->setSizes({300, 520, 260, 280});
}

	// Franja inferior con las tarjetas de estrofas
	strip_ = new QListWidget(central);
strip_->setFlow(QListView::LeftToRight);
strip_->setWrapping(false);
strip_->setMinimumHeight(100);
strip_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Sin barra vertical
strip_->setFocusPolicy(Qt::NoFocus); // Evita el recuadro punteado feo
	strip_->setSpacing(4);
	strip_->setSelectionMode(QAbstractItemView::NoSelection);
	strip_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	rootLayout->addWidget(strip_);

	setCentralWidget(central);
statusBar()->showMessage(QStringLiteral(
	"Flechas: estrofa anterior y siguiente  |  Pad numérico 0 a 9: ir a esa estrofa  |  F9: sin letra"));

cpuLabel_ = new QLabel(this);
cpuLabel_->setStyleSheet(QStringLiteral("margin-right: 8px;"));
statusBar()->addPermanentWidget(cpuLabel_);

auto *cpuTimer = new QTimer(this);
connect(cpuTimer, &QTimer::timeout, this, [this]() {
	static ULARGE_INTEGER lastProcKernel = {0}, lastProcUser = {0};
	static ULARGE_INTEGER lastSysKernel = {0}, lastSysUser = {0};

	FILETIME sysIdle, sysKernel, sysUser;
	FILETIME procCreation, procExit, procKernel, procUser;

	GetSystemTimes(&sysIdle, &sysKernel, &sysUser);
	GetProcessTimes(GetCurrentProcess(), &procCreation, &procExit, &procKernel, &procUser);

	ULARGE_INTEGER sysK, sysU, procK, procU;
	sysK.LowPart = sysKernel.dwLowDateTime; sysK.HighPart = sysKernel.dwHighDateTime;
	sysU.LowPart = sysUser.dwLowDateTime; sysU.HighPart = sysUser.dwHighDateTime;
	procK.LowPart = procKernel.dwLowDateTime; procK.HighPart = procKernel.dwHighDateTime;
	procU.LowPart = procUser.dwLowDateTime; procU.HighPart = procUser.dwHighDateTime;

	if (lastSysKernel.QuadPart != 0) {
		const ULONGLONG procDiff = (procK.QuadPart - lastProcKernel.QuadPart) + (procU.QuadPart - lastProcUser.QuadPart);
		const ULONGLONG sysDiff = (sysK.QuadPart - lastSysKernel.QuadPart) + (sysU.QuadPart - lastSysUser.QuadPart);

		if (sysDiff > 0) {
	const double cpu = (double(procDiff) / double(sysDiff)) * 100.0;
	cpuLabel_->setText(QStringLiteral("CPU: %1%").arg(cpu, 0, 'f', 1));
}
	}
	lastSysKernel = sysK;
	lastSysUser = sysU;
	lastProcKernel = procK;
	lastProcUser = procU;
});
cpuTimer->start(1000);
cpuLabel_->setText(QStringLiteral("CPU: --%"));

	// --- SERVIDOR WEB ---
	webServer_ = new WebServer(this);
	
	// Conexiones directas (sin lambdas ni resta de índices)
	connect(webServer_, &WebServer::nextRequested, this, &MainWindow::nextStanza);
	connect(webServer_, &WebServer::prevRequested, this, &MainWindow::previousStanza);
	connect(webServer_, &WebServer::stanzaRequested, this, &MainWindow::projectStanza);
	connect(webServer_, &WebServer::toggleHiddenRequested, this, &MainWindow::toggleHidden);
	connect(webServer_, &WebServer::songStanzaRequested, this, &MainWindow::projectStanzaFromWeb);

	webServer_->startServer(8080);

	// Conexiones
	connect(songSearch_, &QLineEdit::textChanged, this,
		[this](const QString &) { refreshSongList(); });
	connect(songList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		showSong(item->data(kIdRole).toString());
	});
	connect(songList_, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *item) {
			addToPlaylist(item->data(kIdRole).toString());
		});
	connect(songList_, &QListWidget::customContextMenuRequested, this,
		&MainWindow::songMenu);

// Configuración para la lista central (estrofas completas)
stanzaList_->setContextMenuPolicy(Qt::CustomContextMenu);
// Doble click izquierdo en la lista central -> Proyecta
connect(stanzaList_, &QListWidget::itemDoubleClicked, this,
	&MainWindow::stanzaDoubleClicked);
// Click derecho en la lista central -> Abre el menú
connect(stanzaList_, &QListWidget::customContextMenuRequested, this,
	&MainWindow::stanzaContextMenu);

// Configuración para la barra rápida de abajo
strip_->setContextMenuPolicy(Qt::CustomContextMenu);
// Click izquierdo en la barra rápida -> Proyecta
connect(strip_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
	if (item) projectStanza(strip_->row(item));
});
// Click derecho en la barra rápida -> Abre el menú
connect(strip_, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
	QListWidgetItem *item = strip_->itemAt(pos);
	if (item) showEditMenuForStanza(strip_->row(item), strip_->viewport()->mapToGlobal(pos));
});

	connect(playlist_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		showSong(item->data(kIdRole).toString());
	});
	connect(playlist_, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *item) {
			showSong(item->data(kIdRole).toString());
		});
	connect(playlist_, &QListWidget::customContextMenuRequested, this,
		&MainWindow::playlistMenu);
	connect(clearButton, &QPushButton::clicked, this, [this]() { clearPlaylist(); });

	connect(themeList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
		selectTheme(item->data(kIdRole).toString());
	});
	connect(themeList_, &QListWidget::customContextMenuRequested, this,
		&MainWindow::themeMenu);
	connect(newThemeButton, &QPushButton::clicked, this, [this]() { newTheme(); });

	connect(splitter_, &QSplitter::splitterMoved, this, [this]() {
		updateMonitor();
	});
}

// ---------------------------------------------------------------------------
// Canciones
// ---------------------------------------------------------------------------

void MainWindow::rebuildSearchCache()
{
	searchCache_.clear();
	for (const Song &song : Store::instance().songs) {
		QStringList parts;
		parts << song.title << song.artist;
		// Empezamos desde la estrofa 1 para ignorar la estrofa 0 (Título/Artista)
		for (int i = 1; i < song.stanzas.size(); ++i) {
			parts << song.stanzas[i].text;
		}
		searchCache_.insert(song.id, foldText(parts.join(QLatin1Char('\n'))));
	}
}

void MainWindow::refreshSongList()
{
	const QString query = songSearch_->text().trimmed();
	const QString foldedQuery = foldText(query);

	struct ScoredSong {
		const Song *song;
		int score;
	};
	QVector<ScoredSong> filtered;

	for (const Song &song : Store::instance().songs) {
		int score = 0;
if (!foldedQuery.isEmpty()) {
	QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(foldedQuery));
	if (re.match(foldText(song.title)).hasMatch()) {
		score = 3;
	} else if (re.match(foldText(song.artist)).hasMatch()) {
		score = 2;
	} else if (re.match(searchCache_.value(song.id)).hasMatch()) {
		score = 1;
	} else {
		continue;
	}
} else {
	score = 1;
}
		filtered.append({&song, score});
	}

	std::sort(filtered.begin(), filtered.end(), [](const ScoredSong &a, const ScoredSong &b) {
		if (a.score != b.score) {
			return a.score > b.score;
		}
		return foldText(a.song->title) < foldText(b.song->title);
	});

	if (auto *del = dynamic_cast<HighlightDelegate*>(songList_->itemDelegate())) {
		del->query = query;
	}
	if (auto *del = dynamic_cast<HighlightDelegate*>(stanzaList_->itemDelegate())) {
		del->query = query;
	}

	songList_->clear();
	for (const auto &item : filtered) {
		auto *listItem = new QListWidgetItem(item.song->title, songList_);
		listItem->setData(kIdRole, item.song->id);
		if (!item.song->artist.isEmpty())
			listItem->setToolTip(item.song->artist);
	}
	
	if (!currentSongId_.isEmpty()) {
		stanzaList_->viewport()->update();
	}
}

void MainWindow::showSong(const QString &id)
{
	if (auto *del = dynamic_cast<HighlightDelegate*>(stanzaList_->itemDelegate())) {
		del->query = songSearch_->text().trimmed();
	}
	currentSongId_ = id;
	currentStanza_ = -1;
	stanzaList_->clear();
	
	// Limpiamos los widgets anteriores para evitar superposiciones al cambiar tamaños
	for (int i = 0; i < strip_->count(); ++i) {
		QWidget *w = strip_->itemWidget(strip_->item(i));
		if (w) delete w;
	}
	strip_->clear();

	const Song *song = Store::instance().findSong(id);
	if (!song) {
		songLabel_->setText(QStringLiteral("Elegí una canción de la lista"));
		return;
	}

	songLabel_->setText(song->artist.isEmpty()
				    ? song->title
				    : song->title + QStringLiteral(" - ") + song->artist);

int maxCardHeight = 60; // Altura base
QFont baseFont = strip_->font();

// Buscamos el tamaño global de la canción. Si no tiene, usamos el del programa.
int baseSize = song->defaultCardFontSize;
if (baseSize == -1) {
	baseSize = Store::instance().defaultCardFontSize;
}
baseFont.setPointSize(baseSize);

for (int i = 0; i < song->stanzas.size(); ++i) {
	const QString &text = song->stanzas[i].text;
	
	QFont thisCardFont = baseFont;
	if (song->stanzas[i].cardFontSize != -1) {
		thisCardFont.setPointSize(song->stanzas[i].cardFontSize);
	}
	QFontMetrics fm(thisCardFont);
	
	// Lista central
	new QListWidgetItem(QStringLiteral("%1   %2")
				    .arg(i, 2, 10, QLatin1Char('0'))
				    .arg(text),
			    stanzaList_);

	// Tarjeta de la franja inferior
	auto *card = new QListWidgetItem(strip_);
	if (i == 0) card->setHidden(true); // Ocultamos la tarjeta 0

// Calculamos la altura necesaria para el texto (ancho de 160px por los márgenes)
QRect textBound = fm.boundingRect(QRect(0, 0, 160, 1000), Qt::TextWordWrap, text);
// Aumentamos el margen a 65 para que la letra respire si se achica
int currentCardHeight = textBound.height() + 65; 
if (currentCardHeight > maxCardHeight) {
	maxCardHeight = currentCardHeight;
}

card->setSizeHint(QSize(180, currentCardHeight));

auto *widget = new QWidget();
widget->setObjectName(QStringLiteral("cardWidget")); // Nombre para el borde
widget->setAttribute(Qt::WA_TransparentForMouseEvents); 
widget->setAttribute(Qt::WA_StyledBackground, true); // Necesario para que se dibuje el borde

auto *layout = new QVBoxLayout(widget);
layout->setContentsMargins(6, 6, 6, 6);
layout->setSpacing(2);

auto *numLabel = new QLabel(QString::number(i));
// Forzamos el tamaño con stylesheet para que OBS no lo pise
numLabel->setStyleSheet(QStringLiteral("font-size: %1pt; font-weight: bold; background: transparent;").arg(thisCardFont.pointSizeF() * 0.7));
numLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

auto *textLabel = new QLabel(text);
// Forzamos el tamaño con stylesheet para que OBS no lo pise
textLabel->setStyleSheet(QStringLiteral("font-size: %1pt; background: transparent;").arg(thisCardFont.pointSizeF()));
textLabel->setWordWrap(true);
textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

layout->addWidget(numLabel);
layout->addWidget(textLabel, 1);

		strip_->setItemWidget(card, widget);
	}
	
	// Ajustamos la altura total de la barra para que entre la tarjeta más alta sin scroll
	strip_->setFixedHeight(maxCardHeight + 20);

	updateHighlight();
	updateMonitor();
}

void MainWindow::addSong()
{
	Song song;
	SongEditorDialog dlg(song, true, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	Store &store = Store::instance();
	const Song created = dlg.song();
	store.songs.append(created);
	store.sortSongs();
	store.saveSongs();
	rebuildSearchCache();
	refreshSongList();
	showSong(created.id);
}

void MainWindow::editSong(const QString &id)
{
	Store &store = Store::instance();
	Song *song = store.findSong(id);
	if (!song)
		return;

	SongEditorDialog dlg(*song, false, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	*song = dlg.song();
	store.sortSongs();
	store.saveSongs();
	rebuildSearchCache();
	refreshSongList();
	refreshPlaylist();
	if (currentSongId_ == id)
		showSong(id);
}

void MainWindow::deleteSong(const QString &id)
{
	Store &store = Store::instance();
	const Song *song = store.findSong(id);
	if (!song)
		return;

	const QString question =
		QStringLiteral("¿Eliminar la canción \"%1\"? No se puede deshacer.").arg(song->title);
	if (QMessageBox::question(this, QStringLiteral("Eliminar canción"), question) !=
	    QMessageBox::Yes)
		return;

	for (int i = 0; i < store.songs.size(); ++i) {
		if (store.songs[i].id == id) {
			store.songs.removeAt(i);
			break;
		}
	}
	store.playlist.removeAll(id);
	store.saveSongs();
	store.savePlaylist();
	rebuildSearchCache();
	refreshSongList();
	refreshPlaylist();
	if (currentSongId_ == id)
		showSong(QString());
}

void MainWindow::importTxt()
{
	const QStringList files = QFileDialog::getOpenFileNames(
		this, QStringLiteral("Importar canciones"), QString(),
		QStringLiteral("Archivos de texto (*.txt)"));
	if (files.isEmpty())
		return;

	Store &store = Store::instance();
	int imported = 0;
	int skipped = 0;
	for (const QString &path : files) {
		Song song;
		if (!songFromTxtFile(path, song)) {
			++skipped;
			continue;
		}
		bool exists = false;
		for (const Song &s : store.songs) {
			if (foldText(s.title) == foldText(song.title)) {
				exists = true;
				break;
			}
		}
		if (exists) {
			++skipped;
			continue;
		}
		store.songs.append(song);
		++imported;
	}

	store.sortSongs();
	store.saveSongs();
	rebuildSearchCache();
	refreshSongList();

	QMessageBox::information(
		this, QStringLiteral("Importar canciones"),
		QStringLiteral("Se importaron %1 canciones. Se omitieron %2 (vacías o repetidas).")
			.arg(imported)
			.arg(skipped));
}

void MainWindow::songMenu(const QPoint &pos)
{
	QListWidgetItem *item = songList_->itemAt(pos);
	if (!item)
		return;
	const QString id = item->data(kIdRole).toString();

	QMenu menu(this);
	QAction *addAct = menu.addAction(QStringLiteral("Agregar a lista de reproducción"));
	QAction *editAct = menu.addAction(QStringLiteral("Editar canción"));
	QAction *deleteAct = menu.addAction(QStringLiteral("Eliminar canción"));
	QAction *chosen = menu.exec(songList_->viewport()->mapToGlobal(pos));
	if (chosen == addAct)
		addToPlaylist(id);
	else if (chosen == editAct)
		editSong(id);
	else if (chosen == deleteAct)
		deleteSong(id);
}

// ---------------------------------------------------------------------------
// Estrofas
// ---------------------------------------------------------------------------

void MainWindow::updateHighlight()
{
	for (int i = 0; i < strip_->count() && i < stanzaList_->count(); ++i) {
		const bool active = i == currentStanza_;
		
		// Pintamos la lista central con el fondo verde
		QBrush brush = active ? QBrush(kActiveColor) : QBrush();
		stanzaList_->item(i)->setBackground(brush);
		
		// Pintamos el widget de la barra inferior SOLO con el reborde rojo
		QWidget *w = strip_->itemWidget(strip_->item(i));
		if (w) {
			if (active) {
				// Borde rojo de 2px, forzando fondo transparente
				w->setStyleSheet(QStringLiteral("#cardWidget { border: 2px solid red; background: transparent; border-radius: 4px; }"));
			} else {
				// Sin borde y fondo transparente
				w->setStyleSheet(QStringLiteral("#cardWidget { border: 2px solid transparent; background: transparent; border-radius: 4px; }"));
			}
		}
	}
	
	if (currentStanza_ >= 0 && currentStanza_ < stanzaList_->count()) {
		stanzaList_->scrollToItem(stanzaList_->item(currentStanza_));
		strip_->scrollToItem(strip_->item(currentStanza_));
	}
}

void MainWindow::projectStanza(int index)
{
	const Song *song = Store::instance().findSong(currentSongId_);
	if (!song || index < 0 || index >= song->stanzas.size())
		return;

	currentStanza_ = index;
	LetrasState::instance().setContent(song->stanzas[index].text,
					   song->stanzas[index].fontScale);
	updateHighlight();
	updateMonitor();
}

void MainWindow::projectStanzaFromWeb(const QString &songId, int index)
{
	if (currentSongId_ != songId) {
		showSong(songId);
	}
	projectStanza(index);
}

void MainWindow::nextStanza()
{
	projectStanza(currentStanza_ < 0 ? 0 : currentStanza_ + 1);
}

void MainWindow::previousStanza()
{
	if (currentStanza_ > 0)
		projectStanza(currentStanza_ - 1);
}

void MainWindow::toggleHidden()
{
	LetrasState &state = LetrasState::instance();
	state.setHidden(!state.isHidden());
	updateMonitor();
}

void MainWindow::stanzaDoubleClicked(QListWidgetItem *item)
{
	if (!item) return;
	
	int index = stanzaList_->row(item);
	if (index >= 0) {
		projectStanza(index);
	}
}

void MainWindow::stanzaContextMenu(const QPoint &pos)
{
	QListWidgetItem *item = stanzaList_->itemAt(pos);
	if (!item) return;

	int index = stanzaList_->row(item);
	if (index < 0) return;

	showEditMenuForStanza(index, stanzaList_->viewport()->mapToGlobal(pos));
}

void MainWindow::showEditMenuForStanza(int index, const QPoint &globalPos)
{
	QMenu menu(this);
	QAction *editStanzaAct = menu.addAction(QStringLiteral("Editar texto de esta estrofa"));
	QAction *editSizeAct = menu.addAction(QStringLiteral("Editar tamaño de letra"));
	QAction *editSongAct = menu.addAction(QStringLiteral("Editar canción completa"));

	QAction *chosen = menu.exec(globalPos);
	
	if (chosen == editStanzaAct) {
		editStanzaQuick(index);
	} else if (chosen == editSizeAct) {
		editCardFontSize(index);
	} else if (chosen == editSongAct) {
		editSong(currentSongId_);
	}
}
void MainWindow::editStanzaQuick(int index)
{
	Store &store = Store::instance();
	Song *song = store.findSong(currentSongId_);
	if (!song || index < 0 || index >= song->stanzas.size())
		return;

	Stanza &stanza = song->stanzas[index];

	// Armamos la ventanita rápido
QDialog dlg(this);
dlg.setWindowTitle(QStringLiteral("Edición rápida de estrofa"));
dlg.resize(500, 400); // Le damos un tamaño cómodo para editar

	auto *layout = new QVBoxLayout(&dlg);

	// Editor de texto
	auto *textEdit = new QPlainTextEdit(&dlg);
	textEdit->setPlainText(stanza.text);
	layout->addWidget(new QLabel(QStringLiteral("Texto de la estrofa:"), &dlg));
	layout->addWidget(textEdit);

	// Selector de tamaño
	auto *sizeLayout = new QHBoxLayout();
	auto *sizeSpin = new QSpinBox(&dlg);
	sizeSpin->setRange(20, 400);
	sizeSpin->setSuffix(QStringLiteral(" %"));
	sizeSpin->setValue(stanza.fontScale);
	sizeLayout->addWidget(new QLabel(QStringLiteral("Tamaño:"), &dlg));
	sizeLayout->addWidget(sizeSpin);
	sizeLayout->addStretch(1);
	layout->addLayout(sizeLayout);
// Selector de tamaño individual para esta tarjeta
auto *cardSizeLayout = new QHBoxLayout();
auto *cardSizeSpin = new QSpinBox(&dlg);
cardSizeSpin->setRange(5, 40);
cardSizeSpin->setSuffix(QStringLiteral(" pt"));
auto *useGlobalCheck = new QCheckBox(QStringLiteral("Global"), &dlg);

int currentCardSize = stanza.cardFontSize;
if (currentCardSize == -1) {
	currentCardSize = store.defaultCardFontSize;
	useGlobalCheck->setChecked(true);
	cardSizeSpin->setDisabled(true);
} else {
	useGlobalCheck->setChecked(false);
}
cardSizeSpin->setValue(currentCardSize);

cardSizeLayout->addWidget(new QLabel(QStringLiteral("Letra tarjeta:"), &dlg));
cardSizeLayout->addWidget(cardSizeSpin);
cardSizeLayout->addWidget(useGlobalCheck);
cardSizeLayout->addStretch(1);
layout->addLayout(cardSizeLayout);

// Tamaño Global de todas las tarjetas (ahora por canción)
auto *globalSizeLayout = new QHBoxLayout();
auto *globalSizeSpin = new QSpinBox(&dlg);
globalSizeSpin->setRange(5, 40);
globalSizeSpin->setSuffix(QStringLiteral(" pt"));
int initialGlobal = (song->defaultCardFontSize != -1) ? song->defaultCardFontSize : store.defaultCardFontSize;
globalSizeSpin->setValue(initialGlobal);

globalSizeLayout->addWidget(new QLabel(QStringLiteral("Tamaño global:"), &dlg));
globalSizeLayout->addWidget(globalSizeSpin);
globalSizeLayout->addStretch(1);
layout->addLayout(globalSizeLayout);

connect(useGlobalCheck, &QCheckBox::toggled, cardSizeSpin, &QSpinBox::setDisabled);

	// Botones
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() == QDialog::Accepted) {
		stanza.text = textEdit->toPlainText().trimmed();
		stanza.fontScale = sizeSpin->value();
if (useGlobalCheck->isChecked()) {
	stanza.cardFontSize = -1;
} else {
	stanza.cardFontSize = cardSizeSpin->value();
}

if (song->defaultCardFontSize != globalSizeSpin->value()) {
	song->defaultCardFontSize = globalSizeSpin->value();
	// No hace falta saveSettings, porque saveSongs() ya se llama abajo
}
		
		store.saveSongs();
		rebuildSearchCache();
		
		// Refrescamos la vista
		int currentIdx = currentStanza_;
		showSong(currentSongId_); // Esto rearma las listas
		
		// Si estábamos proyectando esta misma estrofa, la volvemos a proyectar con el cambio
		if (currentIdx == index) {
			projectStanza(index);
		}
	}
}
void MainWindow::editCardFontSize(int index)
{
	Store &store = Store::instance();
	Song *song = store.findSong(currentSongId_);
	if (!song || index < 0 || index >= song->stanzas.size())
		return;

	Stanza &stanza = song->stanzas[index];

	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Tamaño de letra de la tarjeta"));
	dlg.resize(350, 150);

	auto *layout = new QVBoxLayout(&dlg);

	// Tamaño individual para esta tarjeta
	auto *cardSizeLayout = new QHBoxLayout();
	auto *cardSizeSpin = new QSpinBox(&dlg);
	cardSizeSpin->setRange(5, 40);
	cardSizeSpin->setSuffix(QStringLiteral(" pt"));
	auto *useGlobalCheck = new QCheckBox(QStringLiteral("Usar tamaño global"), &dlg);
	
	int currentCardSize = stanza.cardFontSize;
	if (currentCardSize == -1) {
		currentCardSize = store.defaultCardFontSize;
		useGlobalCheck->setChecked(true);
		cardSizeSpin->setDisabled(true);
	} else {
		useGlobalCheck->setChecked(false);
	}
	cardSizeSpin->setValue(currentCardSize);

	cardSizeLayout->addWidget(new QLabel(QStringLiteral("Esta tarjeta:"), &dlg));
	cardSizeLayout->addWidget(cardSizeSpin);
	cardSizeLayout->addWidget(useGlobalCheck);
	layout->addLayout(cardSizeLayout);

	// Tamaño global para todas las tarjetas (por canción)
auto *globalSizeLayout = new QHBoxLayout();
auto *globalSizeSpin = new QSpinBox(&dlg);
globalSizeSpin->setRange(5, 40);
globalSizeSpin->setSuffix(QStringLiteral(" pt"));
int initialGlobal = (song->defaultCardFontSize != -1) ? song->defaultCardFontSize : store.defaultCardFontSize;
globalSizeSpin->setValue(initialGlobal);
	globalSizeLayout->addWidget(new QLabel(QStringLiteral("Tamaño global:"), &dlg));
	globalSizeLayout->addWidget(globalSizeSpin);
	layout->addLayout(globalSizeLayout);

	connect(useGlobalCheck, &QCheckBox::toggled, cardSizeSpin, &QSpinBox::setDisabled);

	// Botones
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() == QDialog::Accepted) {
		if (useGlobalCheck->isChecked()) {
			stanza.cardFontSize = -1;
		} else {
			stanza.cardFontSize = cardSizeSpin->value();
		}

		if (song->defaultCardFontSize != globalSizeSpin->value()) {
	song->defaultCardFontSize = globalSizeSpin->value();
}

		store.saveSongs();
		showSong(currentSongId_); // Refresca las tarjetas
	}
}

// ---------------------------------------------------------------------------
// Lista de reproduccion
// ---------------------------------------------------------------------------

void MainWindow::refreshPlaylist()
{
	Store &store = Store::instance();
	playlist_->clear();
	for (const QString &id : store.playlist) {
		const Song *song = store.findSong(id);
		if (!song)
			continue;
		auto *item = new QListWidgetItem(song->title, playlist_);
		item->setData(kIdRole, id);
		item->setData(kArtistRole, song->artist);
	}
}

void MainWindow::addToPlaylist(const QString &id)
{
	Store &store = Store::instance();
	if (!store.findSong(id) || store.playlist.contains(id))
		return;
	store.playlist.append(id);
	store.savePlaylist();
	refreshPlaylist();
}

void MainWindow::removeFromPlaylist(const QString &id)
{
	Store &store = Store::instance();
	store.playlist.removeAll(id);
	store.savePlaylist();
	refreshPlaylist();
}

void MainWindow::clearPlaylist()
{
	Store &store = Store::instance();
	if (store.playlist.isEmpty())
		return;
	if (QMessageBox::question(this, QStringLiteral("Limpiar lista"),
				  QStringLiteral("¿Querés vaciar la lista de reproducción de hoy?")) !=
	    QMessageBox::Yes)
		return;
	store.playlist.clear();
	store.savePlaylist();
	refreshPlaylist();
}

void MainWindow::playlistMenu(const QPoint &pos)
{
	QListWidgetItem *item = playlist_->itemAt(pos);
	if (!item)
		return;
	const QString id = item->data(kIdRole).toString();

	QMenu menu(this);
	QAction *editAct = menu.addAction(QStringLiteral("Editar canción"));
	QAction *removeAct = menu.addAction(QStringLiteral("Quitar de la lista"));
	QAction *chosen = menu.exec(playlist_->viewport()->mapToGlobal(pos));
	if (chosen == editAct)
		editSong(id);
	else if (chosen == removeAct)
		removeFromPlaylist(id);
}

// ---------------------------------------------------------------------------
// Temas
// ---------------------------------------------------------------------------

void MainWindow::refreshThemes()
{
	Store &store = Store::instance();
	themeList_->clear();
	for (const Theme &t : store.themes) {
		const QImage img = renderStanza(t, QStringLiteral("Texto de ejemplo"), 100, themeThumbWidth_,
				themeThumbWidth_ * 9 / 16, QColor(40, 40, 40));
auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(img)), t.name,
				 themeList_);
item->setData(kIdRole, t.id);
item->setTextAlignment(Qt::AlignCenter);

if (t.id == store.selectedThemeId)
			item->setBackground(QBrush(kActiveColor));
	}
}

void MainWindow::selectTheme(const QString &id)
{
	Store &store = Store::instance();
	if (!store.findTheme(id))
		return;
	store.selectedThemeId = id;
	store.saveThemes();
	LetrasState::instance().setTheme(store.currentTheme());
	refreshThemes();
	updateMonitor();
}

void MainWindow::newTheme()
{
	Store &store = Store::instance();
	Theme t = store.currentTheme();
	t.id = newId();
	t.name = QStringLiteral("Tema nuevo");

	ThemeEditorDialog dlg(t, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	const Theme created = dlg.theme();
	store.themes.append(created);
	selectTheme(created.id);
}

void MainWindow::editTheme(const QString &id)
{
	Store &store = Store::instance();
	Theme *theme = store.findTheme(id);
	if (!theme)
		return;

	ThemeEditorDialog dlg(*theme, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	*theme = dlg.theme();
	store.saveThemes();
	if (store.selectedThemeId == id)
		LetrasState::instance().setTheme(store.currentTheme());
	refreshThemes();
}

void MainWindow::duplicateTheme(const QString &id)
{
	Store &store = Store::instance();
	const Theme *theme = store.findTheme(id);
	if (!theme)
		return;

	Theme copy = *theme;
	copy.id = newId();
	copy.name = theme->name + QStringLiteral(" (copia)");
	store.themes.append(copy);
	store.saveThemes();
	refreshThemes();
}

void MainWindow::deleteTheme(const QString &id)
{
	Store &store = Store::instance();
	if (store.themes.size() <= 1) {
		QMessageBox::information(this, QStringLiteral("Eliminar tema"),
					 QStringLiteral("No se puede eliminar el único tema que hay."));
		return;
	}
	const Theme *theme = store.findTheme(id);
	if (!theme)
		return;

	const QString question = QStringLiteral("¿Eliminar el tema \"%1\"?").arg(theme->name);
	if (QMessageBox::question(this, QStringLiteral("Eliminar tema"), question) !=
	    QMessageBox::Yes)
		return;

	for (int i = 0; i < store.themes.size(); ++i) {
		if (store.themes[i].id == id) {
			store.themes.removeAt(i);
			break;
		}
	}
	if (store.selectedThemeId == id) {
		store.selectedThemeId = store.themes.first().id;
		LetrasState::instance().setTheme(store.currentTheme());
	}
	store.saveThemes();
	refreshThemes();
}

void MainWindow::themeMenu(const QPoint &pos)
{
	QListWidgetItem *item = themeList_->itemAt(pos);
	if (!item)
		return;
	const QString id = item->data(kIdRole).toString();

	QMenu menu(this);
	QAction *editAct = menu.addAction(QStringLiteral("Editar"));
	QAction *duplicateAct = menu.addAction(QStringLiteral("Duplicar"));
	QAction *deleteAct = menu.addAction(QStringLiteral("Eliminar"));
	menu.addSeparator();
	QAction *sizeAct = menu.addAction(QStringLiteral("Editar tamaño"));
	
	QAction *chosen = menu.exec(themeList_->viewport()->mapToGlobal(pos));
	if (chosen == editAct)
		editTheme(id);
	else if (chosen == duplicateAct)
		duplicateTheme(id);
	else if (chosen == deleteAct)
		deleteTheme(id);
	else if (chosen == sizeAct)
		editThemeSize();
}

void MainWindow::editThemeSize()
{
	bool ok;
	int newSize = QInputDialog::getInt(
		this, QStringLiteral("Editar tamaño"),
		QStringLiteral("Ancho del cuadro (píxeles):"), 
		themeThumbWidth_, 100, 400, 10, &ok);
		
	if (!ok || newSize == themeThumbWidth_)
		return;

	themeThumbWidth_ = newSize;
	themeList_->setIconSize(QSize(themeThumbWidth_, themeThumbWidth_ * 9 / 16));
	themeList_->setGridSize(QSize(themeThumbWidth_ + 20, themeThumbWidth_ * 9 / 16 + 40));
	
	// Actualizamos el límite menor de la columna al nuevo tamaño
themeList_->parentWidget()->setMinimumWidth(themeThumbWidth_ + 40);

	refreshThemes();
	updateMonitor();
}

// ---------------------------------------------------------------------------
// Transiciones
// ---------------------------------------------------------------------------

void MainWindow::showQrCode()
{
	QString ip = webServer_->getLocalIP();
	QString url = QStringLiteral("http://%1:8080").arg(ip);

	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Conectar Celular"));
	dlg.setFixedSize(400, 480);

	auto *layout = new QVBoxLayout(&dlg);

	auto *label = new QLabel(QStringLiteral("Escaneá este código QR con tu celular:\n(Asegurate de estar en el mismo Wi-Fi)"), &dlg);
	label->setAlignment(Qt::AlignCenter);
	layout->addWidget(label);

	auto *qrLabel = new QLabel(QStringLiteral("Generando código QR..."), &dlg);
	qrLabel->setAlignment(Qt::AlignCenter);
	qrLabel->setMinimumSize(300, 300);
	layout->addWidget(qrLabel, 1, Qt::AlignCenter);

	auto *urlLabel = new QLabel(url, &dlg);
	urlLabel->setAlignment(Qt::AlignCenter);
	urlLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #4c6e1e; font-size: 14px;"));
	layout->addWidget(urlLabel);

	auto *buttons = new QHBoxLayout();
	auto *copyBtn = new QPushButton(QStringLiteral("Copiar Enlace"), &dlg);
	auto *closeBtn = new QPushButton(QStringLiteral("Cerrar"), &dlg);
	buttons->addWidget(copyBtn);
	buttons->addWidget(closeBtn);
	layout->addLayout(buttons);

	connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
	connect(copyBtn, &QPushButton::clicked, this, [url]() {
		QApplication::clipboard()->setText(url);
	});

	// Generar el QR usando una API gratuita
	QNetworkAccessManager *manager = new QNetworkAccessManager(&dlg);
QString encodedUrl = QString::fromLatin1(QUrl::toPercentEncoding(url));
QString apiUrl = QStringLiteral("http://api.qrserver.com/v1/create-qr-code/?size=300x300&data=%1").arg(encodedUrl);
	QNetworkRequest request{QUrl(apiUrl)};
	QNetworkReply *reply = manager->get(request);
	connect(reply, &QNetworkReply::finished, &dlg, [reply, qrLabel]() {
		if (reply->error() == QNetworkReply::NoError) {
			QPixmap pixmap;
			pixmap.loadFromData(reply->readAll());
			qrLabel->setPixmap(pixmap);
		} else {
			qrLabel->setText(QStringLiteral("No se pudo cargar el QR.\n%1\n\nUsá el enlace de abajo.").arg(reply->errorString()));
		}
		reply->deleteLater();
	});

	dlg.exec();
}

void MainWindow::openTransitions()
{
	Store &store = Store::instance();
	TransitionsDialog dlg(store.transition, store.currentTheme(), this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	store.transition = dlg.settings();
	store.saveSettings();
	LetrasState::instance().setTransition(store.transition);
}

void MainWindow::updateMonitor()
{
	if (!monitorLabel_)
		return;

	int w = monitorLabel_->width();
	if (w < 100) w = 100; // Tamaño mínimo para evitar errores
	int h = w * 9 / 16;   // Calcula la altura para mantener la proporción 16:9
	monitorLabel_->setFixedHeight(h);

	auto snap = LetrasState::instance().snapshot();

	if (snap.hidden) {
		QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
		img.fill(QColor(30, 30, 30));
		QPainter p(&img);
		p.setPen(Qt::white);
		p.setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));
		p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("LETRA OCULTA (F9)"));
		p.end();
		monitorLabel_->setPixmap(QPixmap::fromImage(img));
	} else {
		QImage img = renderStanza(snap.theme, snap.text, snap.fontScale, w, h, QColor(40, 40, 40));
		monitorLabel_->setPixmap(QPixmap::fromImage(img));
	}
}


