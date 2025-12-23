#include "core/object/object.h"
class ImageTexture;
class Control;
class GDREGuiIcons {
	static GDREGuiIcons *singleton;

	HashMap<float, HashMap<StringName, Ref<ImageTexture>>> icons;
	bool initialized;
	Mutex init_lock;

	void init_for_scale(float scale);
	void init();

	Ref<ImageTexture> _get_gdre_icon(const StringName &p_name, float scale = 1.0);

public:
	static Ref<ImageTexture> get_icon(const StringName &p_name, float scale = 1.0);
	static void add_icons_to_theme(Control *p_theme);

	static GDREGuiIcons *get_singleton();

	GDREGuiIcons();
	~GDREGuiIcons();
};
