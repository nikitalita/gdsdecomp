#include "export_report.h"
#include "core/object/class_db.h"

void ExportReport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_exporter", "exporter"), &ExportReport::set_exporter);
	ClassDB::bind_method(D_METHOD("get_exporter"), &ExportReport::get_exporter);
	ClassDB::bind_method(D_METHOD("set_message", "message"), &ExportReport::set_message);
	ClassDB::bind_method(D_METHOD("get_message"), &ExportReport::get_message);
	ClassDB::bind_method(D_METHOD("set_import_info", "import_info"), &ExportReport::set_import_info);
	ClassDB::bind_method(D_METHOD("get_import_info"), &ExportReport::get_import_info);
	ClassDB::bind_method(D_METHOD("get_source_path"), &ExportReport::get_source_path);
	ClassDB::bind_method(D_METHOD("get_new_source_path"), &ExportReport::get_new_source_path);
	ClassDB::bind_method(D_METHOD("set_saved_path", "saved_path"), &ExportReport::set_saved_path);
	ClassDB::bind_method(D_METHOD("get_saved_path"), &ExportReport::get_saved_path);
	ClassDB::bind_method(D_METHOD("set_error", "error"), &ExportReport::set_error);
	ClassDB::bind_method(D_METHOD("get_error"), &ExportReport::get_error);
	ClassDB::bind_method(D_METHOD("set_loss_type", "loss_type"), &ExportReport::set_loss_type);
	ClassDB::bind_method(D_METHOD("get_loss_type"), &ExportReport::get_loss_type);
	ClassDB::bind_method(D_METHOD("set_rewrote_metadata", "rewrote_metadata"), &ExportReport::set_rewrote_metadata);
	ClassDB::bind_method(D_METHOD("get_rewrote_metadata"), &ExportReport::get_rewrote_metadata);
	ClassDB::bind_method(D_METHOD("append_error_messages", "error_message"), &ExportReport::append_error_messages);
	ClassDB::bind_method(D_METHOD("clear_error_messages"), &ExportReport::clear_error_messages);
	ClassDB::bind_method(D_METHOD("set_error_messages", "error_messages"), &ExportReport::set_error_messages);
	ClassDB::bind_method(D_METHOD("get_error_messages"), &ExportReport::get_error_messages);
	ClassDB::bind_method(D_METHOD("append_message_detail", "message_detail"), &ExportReport::append_message_detail);
	ClassDB::bind_method(D_METHOD("clear_message_detail"), &ExportReport::clear_message_detail);
	ClassDB::bind_method(D_METHOD("set_message_detail", "message_detail"), &ExportReport::set_message_detail);
	ClassDB::bind_method(D_METHOD("get_message_detail"), &ExportReport::get_message_detail);
	ClassDB::bind_method(D_METHOD("set_resources_used", "resources_used"), &ExportReport::set_resources_used);
	ClassDB::bind_method(D_METHOD("get_resources_used"), &ExportReport::get_resources_used);
	ClassDB::bind_method(D_METHOD("set_unsupported_format_type", "unsupported_format_type"), &ExportReport::set_unsupported_format_type);
	ClassDB::bind_method(D_METHOD("get_unsupported_format_type"), &ExportReport::get_unsupported_format_type);
	ClassDB::bind_method(D_METHOD("set_extra_info", "extra_info"), &ExportReport::set_extra_info);
	ClassDB::bind_method(D_METHOD("get_extra_info"), &ExportReport::get_extra_info);
	ClassDB::bind_method(D_METHOD("set_download_task_id", "download_task_id"), &ExportReport::set_download_task_id);
	ClassDB::bind_method(D_METHOD("get_download_task_id"), &ExportReport::get_download_task_id);
	ClassDB::bind_method(D_METHOD("set_test_error", "test_error"), &ExportReport::set_test_error);
	ClassDB::bind_method(D_METHOD("get_test_error"), &ExportReport::get_test_error);
	ClassDB::bind_method(D_METHOD("append_test_error_messages", "test_error_messages"), &ExportReport::append_test_error_messages);
	ClassDB::bind_method(D_METHOD("clear_test_error_messages"), &ExportReport::clear_test_error_messages);
	ClassDB::bind_method(D_METHOD("set_test_error_messages", "test_error_messages"), &ExportReport::set_test_error_messages);
	ClassDB::bind_method(D_METHOD("get_test_error_messages"), &ExportReport::get_test_error_messages);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "exporter"), "set_exporter", "get_exporter");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "set_message", "get_message");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "import_info", PROPERTY_HINT_RESOURCE_TYPE, "ImportInfo"), "set_import_info", "get_import_info");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "saved_path"), "set_saved_path", "get_saved_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "error"), "set_error", "get_error");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loss_type"), "set_loss_type", "get_loss_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rewrote_metadata"), "set_rewrote_metadata", "get_rewrote_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "error_messages"), "set_error_messages", "get_error_messages");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "message_detail"), "set_message_detail", "get_message_detail");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "resources_used"), "set_resources_used", "get_resources_used");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "unsupported_format_type"), "set_unsupported_format_type", "get_unsupported_format_type");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "extra_info"), "set_extra_info", "get_extra_info");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "download_task_id"), "set_download_task_id", "get_download_task_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "test_error"), "set_test_error", "get_test_error");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "test_error_messages"), "set_test_error_messages", "get_test_error_messages");
}

Dictionary ExportReport::to_json() const {
	Dictionary ret;
	if (import_info.is_valid()) {
		ret["import_info"] = import_info->to_json();
	}
	if (!exporter.is_empty()) {
		ret["exporter"] = exporter;
	}
	if (!message.is_empty()) {
		ret["message"] = message;
	}
	if (!saved_path.is_empty()) {
		ret["saved_path"] = saved_path;
	}
	if (!resources_used.is_empty()) {
		ret["resources_used"] = resources_used;
	}
	if (!unsupported_format_type.is_empty()) {
		ret["unsupported_format_type"] = unsupported_format_type;
	}
	if (!error_messages.is_empty()) {
		ret["error_messages"] = error_messages;
	}
	if (!message_detail.is_empty()) {
		ret["message_detail"] = message_detail;
	}
	if (!extra_info.is_empty()) {
		ret["extra_info"] = extra_info;
	}
	if (download_task_id != -1) {
		ret["download_task_id"] = download_task_id;
	}
	if (error != OK) {
		ret["error"] = error;
	}
	if (loss_type != ImportInfo::LossType::LOSSLESS) {
		ret["loss_type"] = loss_type;
	}
	if (rewrote_metadata != NOT_DIRTY) {
		ret["rewrote_metadata"] = rewrote_metadata;
	}
	if (test_error != OK) {
		ret["test_error"] = test_error;
	}
	if (!test_error_messages.is_empty()) {
		ret["test_error_messages"] = test_error_messages;
	}
	return ret;
}

Ref<ExportReport> ExportReport::from_json(const Dictionary &p_json) {
	Ref<ExportReport> report = memnew(ExportReport);
	Dictionary import_info_json = p_json.get("import_info", Dictionary());
	if (!import_info_json.is_empty()) {
		report->import_info = ImportInfo::from_json(import_info_json);
	}
	report->exporter = p_json.get("exporter", "");
	report->message = p_json.get("message", "");
	report->saved_path = p_json.get("saved_path", "");
	report->resources_used = p_json.get("resources_used", Vector<String>());
	report->unsupported_format_type = p_json.get("unsupported_format_type", "");
	report->error_messages = p_json.get("error_messages", Vector<String>());
	report->message_detail = p_json.get("message_detail", Vector<String>());
	report->extra_info = p_json.get("extra_info", Dictionary());
	report->download_task_id = p_json.get("download_task_id", -1);
	report->error = p_json.get("error", OK);
	report->loss_type = p_json.get("loss_type", ImportInfo::LossType::LOSSLESS);
	report->rewrote_metadata = p_json.get("rewrote_metadata", NOT_DIRTY);
	report->test_error = p_json.get("test_error", OK);
	report->test_error_messages = p_json.get("test_error_messages", Vector<String>());
	return report;
}

bool ExportReport::is_equal_to(const Ref<ExportReport> &p_export_report) const {
	if (p_export_report.is_null()) {
		return false;
	}
	if (exporter != p_export_report->exporter) {
		return false;
	}
	if (message != p_export_report->message) {
		return false;
	}
	if (saved_path != p_export_report->saved_path) {
		return false;
	}
	if (resources_used != p_export_report->resources_used) {
		return false;
	}
	if (unsupported_format_type != p_export_report->unsupported_format_type) {
		return false;
	}
	if (error_messages != p_export_report->error_messages) {
		return false;
	}
	if (message_detail != p_export_report->message_detail) {
		return false;
	}
	if (extra_info != p_export_report->extra_info) {
		return false;
	}
	if (download_task_id != p_export_report->download_task_id) {
		return false;
	}
	if (error != p_export_report->error) {
		return false;
	}
	if (loss_type != p_export_report->loss_type) {
		return false;
	}
	if (rewrote_metadata != p_export_report->rewrote_metadata) {
		return false;
	}
	if (import_info.is_valid() != p_export_report->import_info.is_valid()) {
		return false;
	}
	if (import_info.is_valid()) {
		return import_info->is_equal_to(p_export_report->import_info);
	}
	if (test_error != p_export_report->test_error) {
		return false;
	}
	if (test_error_messages != p_export_report->test_error_messages) {
		return false;
	}
	return true;
}
