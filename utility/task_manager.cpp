#include "task_manager.h"
#include "utility/common.h"

TaskManager *TaskManager::singleton = nullptr;

TaskManager::TaskManager() {
	singleton = this;
}

TaskManager::~TaskManager() {
	group_id_to_description.clear();
	singleton = nullptr;
}

TaskManager *TaskManager::get_singleton() {
	return singleton;
}

//	void wait_for_group_task_completion(WorkerThreadPool::GroupID p_group_id);
Error TaskManager::wait_for_group_task_completion(WorkerThreadPool::GroupID p_group_id) {
	Error err = OK;
	{
		std::shared_ptr<BaseTemplateTaskData> task;
		bool already_waiting = false;
		bool found = group_id_to_description.modify_if(p_group_id, [&](auto &v) {
			task = v.second;
			already_waiting = task->is_waiting;
			task->is_waiting = true;
		});
		if (!task || !found) {
			return ERR_INVALID_PARAMETER;
		} else if (already_waiting) {
			return ERR_ALREADY_IN_USE;
		}
		if (task->wait_for_completion()) {
			err = ERR_SKIP;
		}
	}
	group_id_to_description.erase(p_group_id);
	return err;
}

void TaskManager::update_progress_bg() {
	group_id_to_description.for_each_m([&](auto &v) {
		if (!v.second->is_waiting) {
			v.second->update_progress();
		}
	});
}

TaskManager::DownloadTaskID TaskManager::add_download_task(const String &p_download_url, const String &p_save_path) {
	return download_thread.add_download_task(p_download_url, p_save_path);
}

Error TaskManager::wait_for_download_task_completion(DownloadTaskID p_task_id) {
	return download_thread.wait_for_task_completion(p_task_id);
}

int TaskManager::DownloadTaskData::get_current_task_step_value() {
	return download_progress * 1000;
}

void TaskManager::DownloadTaskData::run_singlethreaded() {
	callback_data(nullptr);
}

void TaskManager::DownloadTaskData::wait_for_task_completion_internal() {
	WorkerThreadPool::get_singleton()->wait_for_task_completion(task_id);
}

bool TaskManager::DownloadTaskData::is_done() {
	return done;
}

String TaskManager::DownloadTaskData::get_current_task_step_description() {
	return "Downloading " + download_url;
}

WorkerThreadPool::TaskID TaskManager::DownloadTaskData::get_task_id() {
	return task_id;
}

void TaskManager::DownloadTaskData::callback_data(void *p_data) {
	download_error = gdre::download_file_sync(download_url, save_path, &download_progress, &canceled);
	done = true;
}

void TaskManager::DownloadTaskData::start_internal() {
	if (task_id != -1) {
		return;
	}
	if (!singlethreaded) {
		task_id = WorkerThreadPool::get_singleton()->add_template_task(
				this, &TaskManager::DownloadTaskData::callback_data, nullptr, true, get_current_task_step_description());
	} else {
		task_id = abs(rand());
	}
	progress = EditorProgressGDDC::create(nullptr, get_current_task_step_description() + itos(task_id), get_current_task_step_description(), 1000, true);
}

TaskManager::DownloadTaskData::DownloadTaskData(const String &p_download_url, const String &p_save_path) :
		download_url(p_download_url), save_path(p_save_path) {
	auto_start = false;
}

void TaskManager::DownloadQueueThread::main_loop() {
	while (running) {
		DownloadTaskID item;
		if (!queue.try_pop(item)) {
			// if (!waiting) {
			// 	TaskManager::get_singleton()->update_progress_bg();
			// }
			OS::get_singleton()->delay_usec(10000);
			continue;
		}
		std::shared_ptr<BaseTemplateTaskData> task;
		tasks.if_contains(item, [&](auto &v) {
			task = v.second;
			task->start();
		});
		if (!task) {
			continue;
		}
		while (!task->is_done() && !task->is_waiting) {
			task->update_progress();
			OS::get_singleton()->delay_usec(50000);
		}
		while (!task->is_done()) {
			OS::get_singleton()->delay_usec(10000);
		}
		if (task->is_canceled()) {
			// pop off the rest of the queue
			MutexLock lock(write_mutex);
			tasks.for_each_m([&](auto &v) {
				v.second->cancel();
			});
			tasks.clear();
			while (queue.try_pop(item)) {
			}
		}
	}
}

TaskManager::DownloadTaskID TaskManager::DownloadQueueThread::add_download_task(const String &p_download_url, const String &p_save_path) {
	MutexLock lock(write_mutex);
	DownloadTaskID task_id = current_task_id.fetch_add(1);
	tasks.try_emplace(task_id, std::make_shared<DownloadTaskData>(p_download_url, p_save_path));
	queue.try_push(task_id);
	return task_id;
}

Error TaskManager::DownloadQueueThread::wait_for_task_completion(DownloadTaskID p_task_id) {
	waiting = true;
	std::shared_ptr<DownloadTaskData> task;
	bool already_waiting = false;
	bool found = tasks.modify_if(p_task_id, [&](auto &v) {
		task = v.second;
		already_waiting = task->is_waiting;
		task->is_waiting = true;
	});
	if (!task || !found) {
		return ERR_INVALID_PARAMETER;
	} else if (already_waiting) {
		return ERR_ALREADY_IN_USE;
	}
	Error err = OK;
	if (task->wait_for_completion()) {
		err = ERR_SKIP;
	} else {
		err = task->get_download_error();
	}
	tasks.erase(p_task_id);
	waiting = false;
	return err;
}
void TaskManager::DownloadQueueThread::thread_func(void *p_userdata) {
	((DownloadQueueThread *)p_userdata)->main_loop();
}

TaskManager::DownloadQueueThread::DownloadQueueThread() {
	thread = memnew(Thread);
	thread->start(thread_func, this);
}

TaskManager::DownloadQueueThread::~DownloadQueueThread() {
	running = false;
	thread->wait_to_finish();
	memdelete(thread);
}
