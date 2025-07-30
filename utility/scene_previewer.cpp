/**************************************************************************/
/*  mesh_editor_plugin.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "scene_previewer.h"

#include "compat/input_event_parser_v2.h"
#include "core/config/project_settings.h"
#include "main/main.h"
#include "scene/2d/camera_2d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/main/viewport.h"

#include "utility/gdre_settings.h"

void ScenePreviewer3D::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if ((mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
			rot_x -= mm->get_relative().y * 0.01;
			rot_y -= mm->get_relative().x * 0.01;

			rot_x = CLAMP(rot_x, -Math::PI / 2, Math::PI / 2);
			_update_rotation();
		}
	}

	Ref<InputEventMagnifyGesture> mg = p_event;
	if (mg.is_valid()) {
		scale *= mg->get_factor();
		_update_rotation();
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_UP) {
		scale *= 1.1;
		_update_rotation();
	}
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_DOWN) {
		scale *= 0.9;
		_update_rotation();
	}
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && (key->get_keycode() == Key::EQUAL || key->get_keycode() == Key::PLUS)) {
		scale *= 1.1;
		_update_rotation();
	} else if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::MINUS) {
		scale *= 0.9;
		_update_rotation();
	}
}

void ScenePreviewer3D::_update_theme_item_cache() {
	SubViewportContainer::_update_theme_item_cache();
	theme_cache.light_1_icon = get_theme_icon(SNAME("MaterialPreviewLight1"), SNAME("EditorIcons"));
	theme_cache.light_2_icon = get_theme_icon(SNAME("MaterialPreviewLight2"), SNAME("EditorIcons"));
}

void ScenePreviewer3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			light_1_switch->set_button_icon(theme_cache.light_1_icon);
			light_2_switch->set_button_icon(theme_cache.light_2_icon);
		} break;
	}
}

void ScenePreviewer3D::_update_rotation() {
	Transform3D t;
	t.basis.rotate(Vector3(0, 1, 0), -rot_y);
	t.basis.rotate(Vector3(1, 0, 0), -rot_x);
	t.scale(scale);
	rotation->set_transform(t);
	// Transform3D camera_transform = camera->get_transform();
	// camera_transform.origin = center;
	// camera->set_transform(camera_transform);
}

AABB _calculate_aabb_for_scene(Node *p_node, AABB &p_scene_aabb) {
	MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(p_node);
	if (mesh_node && mesh_node->get_mesh().is_valid()) {
		Transform3D accum_xform;
		Node3D *base = mesh_node;
		while (base) {
			accum_xform = base->get_transform() * accum_xform;
			base = Object::cast_to<Node3D>(base->get_parent());
		}

		AABB aabb = accum_xform.xform(mesh_node->get_mesh()->get_aabb());
		p_scene_aabb.merge_with(aabb);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		p_scene_aabb = _calculate_aabb_for_scene(p_node->get_child(i), p_scene_aabb);
	}

	return p_scene_aabb;
}

void ScenePreviewer3D::edit(Ref<PackedScene> p_scene) {
	scene = p_scene;
	root = scene->instantiate();
	auto node_3d = Object::cast_to<Node3D>(root);
	if (!node_3d) {
		ERR_PRINT("Failed to cast scene to Node3D");
		return;
	}
	root = node_3d;
	edit_3d(node_3d);
}

void ScenePreviewer3D::edit_2d(Node *root) {
	// SubViewport *sub_viewport_node = memnew(SubViewport);
	// AABB scene_aabb;
	// scene_aabb = _calculate_aabb_for_scene(root, scene_aabb);

	// sub_viewport_node->set_update_mode(SubViewport::UPDATE_ALWAYS);
	// auto our_size = get_size();
	// sub_viewport_node->set_size(Vector2i(our_size.x, our_size.y));
	// sub_viewport_node->set_transparent_background(false);
	// // Ref<World3D> world;
	// // world.instantiate();
	// // sub_viewport_node->set_world_3d(world);

	// add_child(sub_viewport_node);
	// Ref<Environment> env;
	// env.instantiate();
	// env->set_background(Environment::BG_CLEAR_COLOR);

	// Ref<CameraAttributesPractical> camera_attributes;
	// camera_attributes.instantiate();

	// Node *real_root = memnew(Node);
	// real_root->set_name("Root");
	// sub_viewport_node->add_child(real_root);

	// Camera2D *camera = memnew(Camera2D);
	// camera->set_name("Camera2D");
	// real_root->add_child(camera);

	// camera->set_position(Vector3(0.0, 0.0, 3.0));

	// root->add_child(light);
	// root->add_child(light2);

	// sub_viewport_node->add_child(root);

	// // Calculate the camera and lighting position based on the size of the scene.
	// Vector3 center = scene_aabb.get_center();
	// float camera_size = scene_aabb.get_longest_axis_size();

	// const float cam_rot_x = -Math::PI / 4;
	// const float cam_rot_y = -Math::PI / 4;

	// camera->set_orthogonal(camera_size * 2.0, 0.0001, camera_size * 2.0);

	// Transform3D xf;
	// xf.basis = Basis(Vector3(0, 1, 0), cam_rot_y) * Basis(Vector3(1, 0, 0), cam_rot_x);
	// xf.origin = center;
	// xf.translate_local(0, 0, camera_size);

	// camera->set_transform(xf);

	// Transform3D xform;
	// xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math::PI / 6);
	// xform.basis = Basis().rotated(Vector3(1, 0, 0), Math::PI / 6) * xform.basis;

	// light->set_transform(xform * Transform3D().looking_at(Vector3(-2, -1, -1), Vector3(0, 1, 0)));
	// light2->set_transform(xform * Transform3D().looking_at(Vector3(+1, -1, -2), Vector3(0, 1, 0)));

	// // Update the renderer to get the screenshot.
	// DisplayServer::get_singleton()->process_events();
	// Main::iteration();
	// Main::iteration();

	// // Get the texture.
	// Ref<Texture2D> texture = sub_viewport_node->get_texture();
	// ERR_FAIL_COND_MSG(texture.is_null(), "Failed to get texture from sub_viewport_node.");

	// // Remove the initial scene node.
	// sub_viewport_node->remove_child(p_scene);

	// // Cleanup the viewport.
	// if (sub_viewport_node) {
	// 	if (sub_viewport_node->get_parent()) {
	// 		sub_viewport_node->get_parent()->remove_child(sub_viewport_node);
	// 	}
	// 	sub_viewport_node->queue_free();
	// 	sub_viewport_node = nullptr;
	// }

	// // Now generate the cache image.
	// Ref<Image> img = texture->get_image();
	// if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
	// 	img = img->duplicate();

	// 	int preview_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
	// 	preview_size *= EDSCALE;

	// 	int vp_size = MIN(img->get_width(), img->get_height());
	// 	int x = (img->get_width() - vp_size) / 2;
	// 	int y = (img->get_height() - vp_size) / 2;

	// 	if (vp_size < preview_size) {
	// 		img->crop_from_point(x, y, vp_size, vp_size);
	// 	} else {
	// 		int ratio = vp_size / preview_size;
	// 		int size = preview_size * MAX(1, ratio / 2);

	// 		x = (img->get_width() - size) / 2;
	// 		y = (img->get_height() - size) / 2;

	// 		img->crop_from_point(x, y, size, size);
	// 		img->resize(preview_size, preview_size, Image::INTERPOLATE_LANCZOS);
	// 	}
	// 	img->convert(Image::FORMAT_RGB8);

	// 	String temp_path = EditorPaths::get_singleton()->get_cache_dir();
	// 	String cache_base = ProjectSettings::get_singleton()->globalize_path(p_path).md5_text();
	// 	cache_base = temp_path.path_join("resthumb-" + cache_base);

	// 	post_process_preview(img);
	// 	img->save_png(cache_base + ".png");
	// }
}

void ScenePreviewer3D::edit_3d(Node3D *root) {
	setup_3d();

	rot_x = Math::deg_to_rad(-15.0);
	rot_y = Math::deg_to_rad(30.0);

	AABB aabb;
	aabb = _calculate_aabb_for_scene(root, aabb);
	center = aabb.get_center();
	scale = Vector3(1.0, 1.0, 1.0);
	_update_rotation();
	Transform3D root_transform = root->get_transform();
	root_transform.origin = center;
	rotation->add_child(root);
	root->set_transform(root_transform);

	Vector3 ofs = aabb.get_center();
	float m = aabb.get_longest_axis_size();
	if (m != 0) {
		m = 1.0 / m;
		m *= 0.5;
		Transform3D xform;
		xform.basis.scale(Vector3(m, m, m));
		xform.origin = -xform.basis.xform(ofs); //-ofs*m;
		//xform.origin.z -= aabb.get_longest_axis_size() * 2;
		root->set_transform(xform);
	}
}

void ScenePreviewer3D::reset() {
	if (root) {
		rotation->remove_child(root);
		root->queue_free();
		root = nullptr;
		scene = nullptr;
	}
	if (viewport) {
		remove_child(viewport);
		viewport->queue_free();
		viewport = nullptr;
	}
}

void ScenePreviewer3D::_on_light_1_switch_pressed() {
	light1->set_visible(light_1_switch->is_pressed());
}

void ScenePreviewer3D::_on_light_2_switch_pressed() {
	light2->set_visible(light_2_switch->is_pressed());
}

ScenePreviewer3D::ScenePreviewer3D() {
	setup_3d();
}

void ScenePreviewer3D::setup_3d() {
	viewport = memnew(SubViewport);
	Ref<World3D> world_3d;
	world_3d.instantiate();
	viewport->set_world_3d(world_3d); // Use own world.
	add_child(viewport);
	viewport->set_disable_input(true);
	viewport->set_msaa_3d(Viewport::MSAA_4X);
	set_stretch(true);
	camera = memnew(Camera3D);
	camera->set_transform(Transform3D(Basis(), Vector3(0, 0, 1.1)));
	camera->set_perspective(45, 0.1, 10);
	viewport->add_child(camera);

	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		camera_attributes.instantiate();
		camera->set_attributes(camera_attributes);
	}

	light1 = memnew(DirectionalLight3D);
	light1->set_transform(Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));
	viewport->add_child(light1);

	light2 = memnew(DirectionalLight3D);
	light2->set_transform(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));
	light2->set_color(Color(0.7, 0.7, 0.7));
	viewport->add_child(light2);

	rotation = memnew(Node3D);
	viewport->add_child(rotation);

	set_custom_minimum_size(Size2(1, 150) * GDRESettings::get_singleton()->get_auto_display_scale());

	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);
	hb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 2);

	hb->add_spacer();

	VBoxContainer *vb_light = memnew(VBoxContainer);
	hb->add_child(vb_light);

	light_1_switch = memnew(Button);
	light_1_switch->set_theme_type_variation("PreviewLightButton");
	light_1_switch->set_toggle_mode(true);
	light_1_switch->set_pressed(true);
	light_1_switch->set_accessibility_name(TTRC("First Light"));
	vb_light->add_child(light_1_switch);
	light_1_switch->connect(SceneStringName(pressed), callable_mp(this, &ScenePreviewer3D::_on_light_1_switch_pressed));

	light_2_switch = memnew(Button);
	light_2_switch->set_theme_type_variation("PreviewLightButton");
	light_2_switch->set_toggle_mode(true);
	light_2_switch->set_pressed(true);
	light_2_switch->set_accessibility_name(TTRC("Second Light"));
	vb_light->add_child(light_2_switch);
	light_2_switch->connect(SceneStringName(pressed), callable_mp(this, &ScenePreviewer3D::_on_light_2_switch_pressed));

	rot_x = 0;
	rot_y = 0;
}

void ScenePreviewer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("edit", "mesh"), &ScenePreviewer3D::edit);
	ClassDB::bind_method(D_METHOD("reset"), &ScenePreviewer3D::reset);
}