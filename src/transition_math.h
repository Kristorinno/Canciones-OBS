#pragma once

#include <QString>

#include <algorithm>

// Como se dibujan la estrofa vieja y la nueva en un instante de la transicion.
struct TransitionParams {
	double oldOpacity = 0.0;
	double newOpacity = 1.0;
	double oldDx = 0.0;
	double oldDy = 0.0;
	double newDx = 0.0;
	double newDy = 0.0;
	double oldScale = 1.0;
	double newScale = 1.0;
};

// t va de 0 (recien empieza) a 1 (terminada). w y h son el tamanio del lienzo.
inline TransitionParams computeTransition(const QString &type, double t,
					  double w, double h)
{
	TransitionParams r;
	const double c = std::min(1.0, std::max(0.0, t));
	const double p = c * c * (3.0 - 2.0 * c); // suavizado

	// Por defecto: fundido cruzado.
	r.oldOpacity = 1.0 - p;
	r.newOpacity = p;

	if (type == QLatin1String("none")) {
		r.oldOpacity = 0.0;
		r.newOpacity = 1.0;
	} else if (type == QLatin1String("fade")) {
		// Primero se apaga la vieja y despues aparece la nueva.
		r.oldOpacity = std::max(0.0, 1.0 - 2.0 * p);
		r.newOpacity = std::max(0.0, 2.0 * p - 1.0);
	} else if (type.startsWith(QLatin1String("slide_"))) {
		const bool vertical = type == QLatin1String("slide_up") ||
				      type == QLatin1String("slide_down");
		const double shift = 0.08 * (vertical ? h : w);
		double dirX = 0.0;
		double dirY = 0.0;
		if (type == QLatin1String("slide_up"))
			dirY = -1.0;
		else if (type == QLatin1String("slide_down"))
			dirY = 1.0;
		else if (type == QLatin1String("slide_left"))
			dirX = -1.0;
		else if (type == QLatin1String("slide_right"))
			dirX = 1.0;
		r.oldDx = dirX * shift * p;
		r.oldDy = dirY * shift * p;
		r.newDx = -dirX * shift * (1.0 - p);
		r.newDy = -dirY * shift * (1.0 - p);
	} else if (type == QLatin1String("zoom")) {
		r.oldScale = 1.0 + 0.15 * p;
		r.newScale = 0.85 + 0.15 * p;
	}
	return r;
}
