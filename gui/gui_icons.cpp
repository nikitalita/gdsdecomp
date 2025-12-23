#include "gui_icons.h"
#include "gui/gdre_icons.gen.h"
#include "scene/gui/control.h"
#include "scene/resources/image_texture.h"
#include "scene/theme/theme_db.h"
#ifdef MODULE_SVG_ENABLED
#include "modules/svg/image_loader_svg.h"

#endif

// HashMap<float, HashMap<StringName, Ref<ImageTexture>>> GDREGuiIcons::icons;
// bool GDREGuiIcons::initialized = false;
// BinaryMutex GDREGuiIcons::init_lock;

GDREGuiIcons *GDREGuiIcons::singleton = nullptr;
namespace {
static inline Ref<ImageTexture> generate_icon(int p_index, float scale) {
	ERR_FAIL_INDEX_V(p_index, gdre_icons_count, nullptr);
	Ref<Image> img = memnew(Image);

#ifdef MODULE_SVG_ENABLED
	// Upsample icon generation only if the scale isn't an integer multiplier.
	// Generating upsampled icons is slower, and the benefit is hardly visible
	// with integer scales.
	ImageLoaderSVG img_loader;
	img_loader.create_image_from_string(img, gdre_icons_sources[p_index], scale, false, false);
#endif

	return ImageTexture::create_from_image(img);
}
} //namespace

int get_icon_index(const StringName &p_name) {
	for (int i = 0; i < gdre_icons_count; i++) {
		if (gdre_icons_names[i] == p_name) {
			return i;
		}
	}
	return -1;
}

Ref<ImageTexture> GDREGuiIcons::get_icon(const StringName &p_name, float scale) {
	return singleton->_get_gdre_icon(p_name, scale);
}

Ref<ImageTexture> GDREGuiIcons::_get_gdre_icon(const StringName &p_name, float scale) {
	if (!initialized) {
		init();
	}

	// If scale doesn't exist, initialize it for all icons
	if (!icons.has(scale)) {
		init_for_scale(scale);
	}

	// Check if the icon exists for this scale
	if (icons.has(scale) && icons[scale].has(p_name)) {
		return icons[scale][p_name];
	}

	return nullptr;
}

void GDREGuiIcons::init() {
	MutexLock lock(init_lock);
	if (initialized) {
		return;
	}
	// We have to initialize the svg renderer first because the `svg` module is initialized after the `gdsdecomp` module (alphabetical order).
	Vector<float> scales = { 1.0 };
	auto fallback_scale = ThemeDB::get_singleton()->get_fallback_base_scale();
	if (!scales.has(fallback_scale)) {
		scales.push_back(fallback_scale);
	}
	for (float scale : scales) {
		// Initialize all icons for this scale (lock already held)
		if (!icons.has(scale)) {
			for (int i = 0; i < gdre_icons_count; i++) {
				icons[scale][gdre_icons_names[i]] = generate_icon(i, scale);
			}
		}
	}

	initialized = true;
}

void GDREGuiIcons::init_for_scale(float scale) {
	MutexLock lock(init_lock);

	// Check if this scale is already initialized
	if (icons.has(scale)) {
		return;
	}

	// Initialize all icons for this scale
	for (int i = 0; i < gdre_icons_count; i++) {
		icons[scale][gdre_icons_names[i]] = generate_icon(i, scale);
	}
}

void GDREGuiIcons::add_icons_to_theme(Control *p_theme) {
	auto scale = p_theme->get_theme_default_base_scale();
	for (int i = 0; i < gdre_icons_count; i++) {
		p_theme->add_theme_icon_override(gdre_icons_names[i], get_icon(gdre_icons_names[i], scale));
	}
}

GDREGuiIcons::GDREGuiIcons() {
	singleton = this;
}

GDREGuiIcons::~GDREGuiIcons() {
	singleton = nullptr;
}
