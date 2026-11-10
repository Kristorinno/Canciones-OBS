#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QAction>
#include <QApplication>
#include <QMetaObject>
#include <QFontDatabase>
#include <QDir>

#include "letras_source.h"
#include "letras_state.h"
#include "main_window.h"
#include "store.h"

OBS_DECLARE_MODULE()

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Canciones OBS: letras de canciones sobre el lienzo de OBS";
}

namespace {

MainWindow *g_window = nullptr;
obs_hotkey_id g_hotkey = OBS_INVALID_HOTKEY_ID;
const char *const kHotkeySaveKey = "canciones_obs_abrir_ventana";

// Abre la ventana (o la trae al frente si ya estaba abierta).
// Si ya está en primer plano, la minimiza. Siempre se llama desde el hilo de la interfaz.
void show_main_window()
{
	if (!g_window) {
		g_window = new MainWindow(nullptr);
		g_window->openWindow();
		return;
	}

	// Si la ventana ya está visible y activa (primer plano), la minimizamos.
	if (g_window->isVisible() && g_window->isActiveWindow()) {
		g_window->showMinimized();
	} else {
		g_window->openWindow();
	}
}

// Atajo de teclado registrado en OBS (Ajustes, Atajos de teclado).
// OBS lo llama desde otro hilo, asi que se pasa al hilo de la interfaz.
void hotkey_callback(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	QMetaObject::invokeMethod(
		qApp, []() { show_main_window(); }, Qt::QueuedConnection);
}

// Asigna F4 como tecla predeterminada del atajo.
void load_default_hotkey()
{
	obs_data_array_t *bindings = obs_data_array_create();
	obs_data_t *binding = obs_data_create();
	obs_data_set_string(binding, "key", "OBS_KEY_F4");
	obs_data_set_bool(binding, "control", false);
	obs_data_set_bool(binding, "alt", false);
	obs_data_array_push_back(bindings, binding);
	obs_data_release(binding);
	obs_hotkey_load(g_hotkey, bindings);
	obs_data_array_release(bindings);
}

// Guarda y carga la tecla asignada al atajo, para que no se pierda al
// cerrar OBS. Si todavia no hay una tecla guardada, usa la predeterminada.
void save_callback(obs_data_t *save_data, bool saving, void *)
{
	if (g_hotkey == OBS_INVALID_HOTKEY_ID)
		return;

	if (saving) {
		obs_data_array_t *arr = obs_hotkey_save(g_hotkey);
		obs_data_set_array(save_data, kHotkeySaveKey, arr);
		obs_data_array_release(arr);
	} else if (obs_data_has_user_value(save_data, kHotkeySaveKey)) {
		obs_data_array_t *arr = obs_data_get_array(save_data, kHotkeySaveKey);
		obs_hotkey_load(g_hotkey, arr);
		obs_data_array_release(arr);
	} else {
		load_default_hotkey();
	}
}

// Al cerrar OBS se destruye la ventana, para que no quede nada colgado.
void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		delete g_window;
		g_window = nullptr;
	}
}

} // namespace

bool obs_module_load(void)
{
	// Datos guardados: canciones, lista, temas y ajustes.
	Store::instance().load();

QDir fontsDir(Store::instance().fontsPath());
QStringList fontFiles = fontsDir.entryList(QStringList() << "*.ttf" << "*.otf", QDir::Files);
for (const QString &f : fontFiles) {
	QFontDatabase::addApplicationFont(fontsDir.absoluteFilePath(f));
}

LetrasState::instance().setTheme(Store::instance().currentTheme());
LetrasState::instance().setTransition(Store::instance().transition);

register_letras_source();

	// Entrada "Canciones OBS" en el menu Herramientas.
	auto *action = static_cast<QAction *>(
		obs_frontend_add_tools_menu_qaction("Canciones OBS"));
	QObject::connect(action, &QAction::triggered,
			 []() { show_main_window(); });

	// Atajo para abrir la ventana.
	g_hotkey = obs_hotkey_register_frontend("canciones_obs.abrir_ventana",
						"Canciones OBS: abrir ventana",
						hotkey_callback, nullptr);

	obs_frontend_add_save_callback(save_callback, nullptr);
	obs_frontend_add_event_callback(frontend_event, nullptr);

	blog(LOG_INFO, "[Canciones OBS] plugin cargado");
	return true;
}

void obs_module_unload(void)
{
	delete g_window;
	g_window = nullptr;
}
