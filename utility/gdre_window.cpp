#include "gdre_window.h"
#include "utility/gdre_progress.h"

GDREWindow::GDREWindow() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->add_host_window(this);
	}
}

GDREWindow::~GDREWindow() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->remove_host_window(this);
	}
}

void GDREWindow::_notification(int p_what) {
	if (!next_process_calls.is_empty()) {
		auto calls = next_process_calls;
		next_process_calls.clear();
		for (const auto &callable : calls) {
			callable.call();
		}
	}
}

void GDREWindow::call_on_next_process(const Callable &p_callable) {
	next_process_calls.push_back(p_callable);
	if (!is_processing()) {
		set_process(true);
	}
}

GDREAcceptDialogBase::GDREAcceptDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->add_host_window(this);
	}

}

GDREAcceptDialogBase::~GDREAcceptDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->remove_host_window(this);
	}
}


void GDREAcceptDialogBase::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		if (!next_process_calls.is_empty()) {
			auto calls = next_process_calls;
			next_process_calls.clear();
			for (const auto &callable : calls) {
				callable.call();
			}
		}
	}
}

void GDREAcceptDialogBase::call_on_next_process(const Callable &p_callable) {
	next_process_calls.push_back(p_callable);
	if (!is_processing()) {
		set_process(true);
	}
}


void GDREWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("call_on_next_process", "p_callable"), &GDREWindow::call_on_next_process);
}

void GDREAcceptDialogBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("call_on_next_process", "p_callable"), &GDREAcceptDialogBase::call_on_next_process);
}
