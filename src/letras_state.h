#pragma once

#include <cstdint>
#include <mutex>

#include <QString>

#include "data_model.h"

// Estado compartido entre la ventana "Canciones OBS" (hilo de la interfaz)
// y la fuente "Letras" (hilo de graficos de OBS).
class LetrasState {
public:
	struct Snapshot {
		QString text;
		bool hidden = false;
		int fontScale = 100;
		Theme theme;
		TransitionSettings transition;
		// Cambia al proyectar otra estrofa o al ocultar/mostrar la letra
		// (dispara la transicion).
		uint64_t contentVersion = 0;
		// Cambia al cambiar el tema (se redibuja sin transicion).
		uint64_t styleVersion = 0;
	};

	static LetrasState &instance()
	{
		static LetrasState state;
		return state;
	}

	// Proyecta una estrofa. Si la letra estaba oculta, vuelve a mostrarse.
	void setContent(const QString &text, int fontScale)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		text_ = text;
		fontScale_ = fontScale;
		hidden_ = false;
		contentVersion_++;
	}

	// Oculta o muestra la letra sin perder la estrofa actual (tecla F9).
	void setHidden(bool hidden)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		hidden_ = hidden;
		contentVersion_++;
	}

	bool isHidden()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return hidden_;
	}

	void setTheme(const Theme &theme)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		theme_ = theme;
		styleVersion_++;
	}

	void setTransition(const TransitionSettings &transition)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		transition_ = transition;
	}

	Snapshot snapshot()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		Snapshot s;
		s.text = text_;
		s.hidden = hidden_;
		s.fontScale = fontScale_;
		s.theme = theme_;
		s.transition = transition_;
		s.contentVersion = contentVersion_;
		s.styleVersion = styleVersion_;
		return s;
	}

private:
	std::mutex mutex_;
	QString text_;
	bool hidden_ = false;
	int fontScale_ = 100;
	Theme theme_;
	TransitionSettings transition_;
	uint64_t contentVersion_ = 1;
	uint64_t styleVersion_ = 1;
};
