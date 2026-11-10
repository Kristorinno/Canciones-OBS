#include "letras_source.h"
#include "letras_state.h"
#include "text_renderer.h"
#include "transition_math.h"

#include <obs-module.h>
#include <util/platform.h>

#include <QImage>
#include <QString>

namespace {

// Efecto propio para dibujar la textura con opacidad, desplazamiento y zoom.
// El desplazamiento y el zoom se aplican en el shader, asi no dependen de
// la pila de matrices de OBS.
const char *kOpacityEffect = R"EFFECT(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float opacity;
uniform float zoom;
uniform float2 center;
uniform float2 offset;

sampler_state def_sampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertInOut {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertInOut VSMain(VertInOut vert_in)
{
	VertInOut vert_out;
	float2 p = (vert_in.pos.xy - center) * zoom + center + offset;
	vert_out.pos = mul(float4(p, vert_in.pos.z, 1.0), ViewProj);
	vert_out.uv = vert_in.uv;
	return vert_out;
}

float4 PSMain(VertInOut vert_in) : TARGET
{
	return image.Sample(def_sampler, vert_in.uv) * opacity;
}

technique Draw
{
	pass
	{
		vertex_shader = VSMain(vert_in);
		pixel_shader  = PSMain(vert_in);
	}
}
)EFFECT";

gs_effect_t *g_opacityEffect = nullptr;
bool g_opacityEffectTried = false;

// Se crea la primera vez que hace falta, dentro del contexto grafico.
gs_effect_t *get_opacity_effect()
{
	if (!g_opacityEffectTried) {
		g_opacityEffectTried = true;
		char *error = nullptr;
		g_opacityEffect = gs_effect_create(
			kOpacityEffect, "canciones_obs_opacity.effect", &error);
		if (!g_opacityEffect) {
			blog(LOG_WARNING,
			     "[Canciones OBS] no se pudo crear el efecto de opacidad: %s",
			     error ? error : "sin detalle");
		}
		bfree(error);
	}
	return g_opacityEffect;
}

struct LetrasSource {
	obs_source_t *source = nullptr;
	gs_texture_t *cur = nullptr;  // estrofa actual
	gs_texture_t *prev = nullptr; // estrofa anterior (solo durante la transicion)
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t contentVersion = 0;
	uint64_t styleVersion = 0;
	bool initialized = false;
	bool transActive = false;
	uint64_t transStartNs = 0;
	int transDurationMs = 0;
	QString transType;
};

// Tamanio del lienzo base de OBS (normalmente 1920 por 1080).
bool get_canvas_size(uint32_t &w, uint32_t &h)
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return false;
	w = ovi.base_width;
	h = ovi.base_height;
	return true;
}

void destroy_tex(gs_texture_t *&tex)
{
	if (tex) {
		gs_texture_destroy(tex);
		tex = nullptr;
	}
}

// Dibuja la estrofa en una imagen transparente y la sube como textura.
gs_texture_t *make_texture(const LetrasState::Snapshot &snap, uint32_t w,
			   uint32_t h)
{
	if (snap.hidden || snap.text.trimmed().isEmpty() || w == 0 || h == 0)
		return nullptr;

	const QImage img = renderStanza(snap.theme, snap.text, snap.fontScale,
					int(w), int(h));
	const uint8_t *data = img.constBits();
	return gs_texture_create(w, h, GS_BGRA, 1, &data, 0);
}

void draw_texture(gs_texture_t *tex, uint32_t w, uint32_t h, double opacity,
		  double dx, double dy, double zoom)
{
	gs_effect_t *effect = get_opacity_effect();
	const bool custom = effect != nullptr;
	if (!custom)
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, tex);

	if (custom) {
		gs_effect_set_float(gs_effect_get_param_by_name(effect, "opacity"),
				    float(opacity));
		gs_effect_set_float(gs_effect_get_param_by_name(effect, "zoom"),
				    float(zoom));
		struct vec2 center;
		vec2_set(&center, float(w) / 2.0f, float(h) / 2.0f);
		gs_effect_set_vec2(gs_effect_get_param_by_name(effect, "center"),
				   &center);
		struct vec2 offset;
		vec2_set(&offset, float(dx), float(dy));
		gs_effect_set_vec2(gs_effect_get_param_by_name(effect, "offset"),
				   &offset);
	}

	// La imagen esta en alfa premultiplicado.
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(tex, 0, w, h);
	gs_blend_state_pop();
}

const char *letras_name(void *)
{
	return "Letras";
}

void *letras_create(obs_data_t *, obs_source_t *source)
{
	auto *s = new LetrasSource;
	s->source = source;
	return s;
}

void letras_destroy(void *data)
{
	auto *s = static_cast<LetrasSource *>(data);
	if (s->cur || s->prev) {
		obs_enter_graphics();
		destroy_tex(s->cur);
		destroy_tex(s->prev);
		obs_leave_graphics();
	}
	delete s;
}

uint32_t letras_width(void *)
{
	uint32_t w = 1920, h = 1080;
	get_canvas_size(w, h);
	return w;
}

uint32_t letras_height(void *)
{
	uint32_t w = 1920, h = 1080;
	get_canvas_size(w, h);
	return h;
}

obs_properties_t *letras_properties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(
		props, "info",
		"Esta fuente se controla desde Herramientas, Canciones OBS.",
		OBS_TEXT_INFO);
	return props;
}

void letras_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	auto *s = static_cast<LetrasSource *>(data);

	uint32_t w = 0, h = 0;
	if (!get_canvas_size(w, h))
		return;

	const LetrasState::Snapshot snap = LetrasState::instance().snapshot();

	if (!s->initialized || w != s->width || h != s->height) {
		// Primera vez o cambio el tamanio del lienzo: se rehace todo.
		destroy_tex(s->cur);
		destroy_tex(s->prev);
		s->transActive = false;
		s->cur = make_texture(snap, w, h);
		s->initialized = true;
	} else if (snap.contentVersion != s->contentVersion) {
		// Otra estrofa (o letra oculta o mostrada): con transicion.
		gs_texture_t *next = make_texture(snap, w, h);
		destroy_tex(s->prev);
		if (snap.transition.type != QLatin1String("none") &&
		    snap.transition.durationMs > 0 && (s->cur || next)) {
			s->prev = s->cur;
			s->cur = next;
			s->transActive = true;
			s->transStartNs = os_gettime_ns();
			s->transDurationMs = snap.transition.durationMs;
			s->transType = snap.transition.type;
		} else {
			destroy_tex(s->cur);
			s->cur = next;
			s->transActive = false;
		}
	} else if (snap.styleVersion != s->styleVersion) {
		// Cambio el tema: se redibuja sin transicion.
		destroy_tex(s->cur);
		s->cur = make_texture(snap, w, h);
	}

	s->width = w;
	s->height = h;
	s->contentVersion = snap.contentVersion;
	s->styleVersion = snap.styleVersion;

	double progress = 1.0;
	if (s->transActive) {
		const double elapsedMs =
			double(os_gettime_ns() - s->transStartNs) / 1000000.0;
		progress = elapsedMs / double(qMax(1, s->transDurationMs));
		if (progress >= 1.0) {
			destroy_tex(s->prev);
			s->transActive = false;
			progress = 1.0;
		}
	}

	if (s->transActive) {
		const TransitionParams tp = computeTransition(
			s->transType, progress, double(w), double(h));
		if (s->prev && tp.oldOpacity > 0.001)
			draw_texture(s->prev, w, h, tp.oldOpacity, tp.oldDx,
				     tp.oldDy, tp.oldScale);
		if (s->cur && tp.newOpacity > 0.001)
			draw_texture(s->cur, w, h, tp.newOpacity, tp.newDx,
				     tp.newDy, tp.newScale);
	} else if (s->cur) {
		draw_texture(s->cur, w, h, 1.0, 0.0, 0.0, 1.0);
	}
}

} // namespace

void register_letras_source()
{
	obs_source_info info = {};
	info.id = "canciones_obs_letras";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = letras_name;
	info.create = letras_create;
	info.destroy = letras_destroy;
	info.get_width = letras_width;
	info.get_height = letras_height;
	info.get_properties = letras_properties;
	info.video_render = letras_render;
	obs_register_source(&info);
}
