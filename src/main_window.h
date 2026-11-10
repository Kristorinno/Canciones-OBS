#pragma once

#include <QHash>
#include <QMainWindow>
#include <QString>

class QCloseEvent;
class QEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPoint;
class QSplitter;
class WebServer;

// Ventana principal "Canciones OBS": canciones cargadas, estrofas de la
// cancion elegida, lista de reproduccion del dia, temas y franja de estrofas.
class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	// Abre la ventana (restaura tamanio y posicion, o la maximiza la primera vez).
	void openWindow();

protected:
	void closeEvent(QCloseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	

private:
	void buildUi();

	// Canciones
	void rebuildSearchCache();
	void refreshSongList();
	void showSong(const QString &id);
	void addSong();
	void editSong(const QString &id);
	void deleteSong(const QString &id);
	void importTxt();
	void songMenu(const QPoint &pos);

// Estrofas
void updateHighlight();
void projectStanza(int index);
void projectStanzaFromWeb(const QString &songId, int index);
void nextStanza();
void previousStanza();
void toggleHidden();
void stanzaDoubleClicked(QListWidgetItem *item);
void stanzaContextMenu(const QPoint &pos);
void showEditMenuForStanza(int index, const QPoint &globalPos);
void editStanzaQuick(int index);
void editCardFontSize(int index);

	// Lista de reproduccion
	void refreshPlaylist();
	void addToPlaylist(const QString &id);
	void removeFromPlaylist(const QString &id);
	void clearPlaylist();
	void playlistMenu(const QPoint &pos);

	// Temas
	void refreshThemes();
	void selectTheme(const QString &id);
	void newTheme();
	void editTheme(const QString &id);
	void duplicateTheme(const QString &id);
	void deleteTheme(const QString &id);
	void themeMenu(const QPoint &pos);

	void openTransitions();
	void showQrCode();

	QString currentSongId_;
	int currentStanza_ = -1;
	QHash<QString, QString> searchCache_;

	QLineEdit *songSearch_ = nullptr;
	QListWidget *songList_ = nullptr;
	QLabel *songLabel_ = nullptr;
	QListWidget *stanzaList_ = nullptr;
	QListWidget *playlist_ = nullptr;
	QListWidget *themeList_ = nullptr;
	QListWidget *strip_ = nullptr;
	QLabel *cpuLabel_ = nullptr;
	QLabel *monitorLabel_ = nullptr;
	WebServer *webServer_ = nullptr;
QSplitter *splitter_ = nullptr;
int themeThumbWidth_ = 220;

	void updateMonitor();
	void editThemeSize();
};
