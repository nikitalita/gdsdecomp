/**************************************************************************/
/*  http_request.cpp                                                      */
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

#include "http_requester.h"

#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/stream_peer_gzip.h"
#include "core/os/os.h"

#include "common.h"

Error HTTPRequester::_request() {
	return client->connect_to_host(url, port, use_tls ? tls_options : nullptr);
}

Error HTTPRequester::_parse_url(const String &p_url) {
	use_tls = false;
	request_string = "";
	port = 80;
	request_sent = false;
	got_response = false;
	body_len = -1;
	body.clear();
	downloaded.set(0);
	final_body_size.set(0);
	redirections = 0;
	full_url = p_url;
	String scheme;
	String fragment;
	Error err = p_url.parse_url(scheme, url, port, request_string, fragment);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Error parsing URL: '%s'.", p_url));

	if (scheme == "https://") {
		use_tls = true;
	} else if (scheme != "http://") {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, vformat("Invalid URL scheme: '%s'.", scheme));
	}

	if (port == 0) {
		port = use_tls ? 443 : 80;
	}
	if (request_string.is_empty()) {
		request_string = "/";
	}
	return OK;
}

bool HTTPRequester::has_header(const PackedStringArray &p_headers, const String &p_header_name) {
	bool exists = false;

	String lower_case_header_name = p_header_name.to_lower();
	for (int i = 0; i < p_headers.size() && !exists; i++) {
		String sanitized = p_headers[i].strip_edges().to_lower();
		if (sanitized.begins_with(lower_case_header_name)) {
			exists = true;
		}
	}

	return exists;
}

String HTTPRequester::get_header_value(const PackedStringArray &p_headers, const String &p_header_name) {
	String value = "";

	String lowwer_case_header_name = p_header_name.to_lower();
	for (int i = 0; i < p_headers.size(); i++) {
		if (p_headers[i].find_char(':') > 0) {
			Vector<String> parts = p_headers[i].split(":", false, 1);
			if (parts.size() > 1 && parts[0].strip_edges().to_lower() == lowwer_case_header_name) {
				value = parts[1].strip_edges();
				break;
			}
		}
	}

	return value;
}

Error HTTPRequester::start_request(const String &p_url, const Vector<String> &p_custom_headers, HTTPClient::Method p_method, const String &p_request_data) {
	// Copy the string into a raw buffer.
	Vector<uint8_t> raw_data;

	CharString charstr = p_request_data.utf8();
	size_t len = charstr.length();
	if (len > 0) {
		raw_data.resize(len);
		uint8_t *w = raw_data.ptrw();
		memcpy(w, charstr.ptr(), len);
	}

	return start_request_raw(p_url, p_custom_headers, p_method, raw_data);
}

Error HTTPRequester::start_request_raw(const String &p_url, const Vector<String> &p_custom_headers, HTTPClient::Method p_method, const Vector<uint8_t> &p_request_data_raw) {
	// ERR_FAIL_COND_V(!is_inside_tree(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V_MSG(requesting, ERR_BUSY, "HTTPRequester is processing a request. Wait for completion or cancel it before attempting a new one.");

	_reset();
	start_time = OS::get_singleton()->get_unix_time();
	current_status_start_time = start_time;

	method = p_method;

	Error err = _parse_url(p_url);
	if (err) {
		return err;
	}

	headers = p_custom_headers;

	if (accept_gzip) {
		// If the user has specified an Accept-Encoding header, don't overwrite it.
		if (!has_header(headers, "Accept-Encoding")) {
			headers.push_back("Accept-Encoding: gzip, deflate");
		}
	}

	request_data = p_request_data_raw;

	requesting = true;

	if (use_threads.is_set()) {
		thread_done.clear();
		thread_request_quit.clear();
		client->set_blocking_mode(true);
		thread.start(_thread_func, this);
	} else {
		client->set_blocking_mode(false);
		err = _request();
		if (err != OK) {
			_request_done(RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray());
			return ERR_CANT_CONNECT;
		}
		previous_status = client->get_status();

		// set_process_internal(true);
	}

	return OK;
}

void HTTPRequester::_thread_func(void *p_userdata) {
	HTTPRequester *hr = static_cast<HTTPRequester *>(p_userdata);

	Error err = hr->_request();

	if (err != OK) {
		hr->_request_done(RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray());
	} else {
		hr->previous_status = hr->client->get_status();
		while (!hr->thread_request_quit.is_set()) {
			bool exit = hr->poll();
			if (exit) {
				break;
			}
			OS::get_singleton()->delay_usec(1);
		}
	}

	hr->thread_done.set();
}

void HTTPRequester::cancel_request() {
	if (!requesting) {
		return;
	}

	if (!use_threads.is_set()) {
		// set_process_internal(false);
	} else {
		thread_request_quit.set();
		if (thread.is_started()) {
			thread.wait_to_finish();
		}
	}

	if (file.is_valid()) {
		file.unref();
		if (!download_complete) {
			DirAccess::remove_absolute(download_to_file);
		}
	}

	decompressor.unref();
	client->close();
	requesting = false;
}

bool HTTPRequester::_is_content_header(const String &p_header) const {
	return (p_header.begins_with("content-type:") || p_header.begins_with("content-length:") || p_header.begins_with("content-location:") || p_header.begins_with("content-encoding:") ||
			p_header.begins_with("transfer-encoding:") || p_header.begins_with("connection:") || p_header.begins_with("authorization:"));
}

bool HTTPRequester::_is_method_safe() const {
	return (method == HTTPClient::METHOD_GET || method == HTTPClient::METHOD_HEAD || method == HTTPClient::METHOD_OPTIONS || method == HTTPClient::METHOD_TRACE);
}

Error HTTPRequester::_get_redirect_headers(Vector<String> *r_headers) {
	for (const String &E : headers) {
		const String h = E.to_lower();
		// We strip content headers when changing a redirect to GET.
		if (!_is_content_header(h)) {
			r_headers->push_back(E);
		}
	}
	return OK;
}

bool HTTPRequester::_is_automatic_redirect() const {
	if (unlikely(method == HTTPClient::METHOD_CONNECT)) {
		// Never automatically redirect CONNECT requests.
		return false;
	} else if (response_code == 301 || response_code == 302 || response_code == 303 || response_code == 305) {
		// We change unsafe methods to GET for 301, 302, and 303, so these are always redirected.
		// 305 is deprecated and treated as equivalent to 302.
		return true;
	} else if ((response_code == 307 || response_code == 308) && _is_method_safe()) {
		// We only automatically redirect for safe methods on method-preserving status codes.
		return true;
	} else {
		return false;
	}
}

bool HTTPRequester::_handle_response(bool *ret_value) {
	if (!client->has_response()) {
		_request_done(RESULT_NO_RESPONSE, 0, PackedStringArray(), PackedByteArray());
		*ret_value = true;
		return true;
	}

	got_response = true;
	response_code = client->get_response_code();
	List<String> rheaders;
	client->get_response_headers(&rheaders);
	response_headers.clear();
	downloaded.set(0);
	final_body_size.set(0);
	decompressor.unref();

	for (const String &E : rheaders) {
		response_headers.push_back(E);
	}

	if (_is_automatic_redirect()) {
		// Follow redirect.

		if (max_redirects >= 0 && redirections >= max_redirects) {
			_request_done(RESULT_REDIRECT_LIMIT_REACHED, response_code, response_headers, PackedByteArray());
			*ret_value = true;
			return true;
		}

		String new_request;

		for (const String &E : rheaders) {
			if (E.to_lower().begins_with("location: ")) {
				new_request = E.substr(9).strip_edges();
			}
		}

		if (!new_request.is_empty()) {
			// Process redirect.
			client->close();
			int new_redirs = redirections + 1; // Because _request() will clear it.
			Error err;
			if (new_request.begins_with("http")) {
				// New url, new request.
				_parse_url(new_request);
			} else {
				request_string = new_request;
			}

			err = _request();
			if (err == OK) {
				request_sent = false;
				got_response = false;
				body_len = -1;
				body.clear();
				downloaded.set(0);
				final_body_size.set(0);
				redirections = new_redirs;
				*ret_value = false;
				if (!_is_method_safe()) {
					// 301, 302, and 303 are changed to GET for unsafe methods.
					// See: https://www.rfc-editor.org/rfc/rfc9110#section-15.4-3.1
					method = HTTPClient::METHOD_GET;
					// Content headers should be dropped if changing method.
					// See: https://www.rfc-editor.org/rfc/rfc9110#section-15.4-6.2.1
					Vector<String> req_headers;
					_get_redirect_headers(&req_headers);
					headers = req_headers;
				}
				return true;
			}
		}
	}

	// Check if we need to start streaming decompression.
	String content_encoding;
	if (accept_gzip) {
		content_encoding = get_header_value(response_headers, "Content-Encoding").to_lower();
	}
	if (content_encoding == "gzip") {
		decompressor.instantiate();
		decompressor->start_decompression(false, get_download_chunk_size());
	} else if (content_encoding == "deflate") {
		decompressor.instantiate();
		decompressor->start_decompression(true, get_download_chunk_size());
	}

	return false;
}

bool HTTPRequester::_update_connection() {
	auto current_status = client->get_status();
	if (previous_status != current_status) {
		current_status_start_time = OS::get_singleton()->get_unix_time();
		previous_status = current_status;
	}
	switch (current_status) {
		case HTTPClient::STATUS_DISCONNECTED: {
			_request_done(RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray());
			return true; // End it, since it's disconnected.
		} break;
		case HTTPClient::STATUS_RESOLVING: {
			client->poll();
			// Must wait.
			return false;
		} break;
		case HTTPClient::STATUS_CANT_RESOLVE: {
			_request_done(RESULT_CANT_RESOLVE, 0, PackedStringArray(), PackedByteArray());
			return true;

		} break;
		case HTTPClient::STATUS_CONNECTING: {
			client->poll();
			// Must wait.
			return false;
		} break; // Connecting to IP.
		case HTTPClient::STATUS_CANT_CONNECT: {
			_request_done(RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray());
			return true;

		} break;
		case HTTPClient::STATUS_CONNECTED: {
			if (request_sent) {
				if (!got_response) {
					// No body.

					bool ret_value;

					if (_handle_response(&ret_value)) {
						return ret_value;
					}

					_request_done(RESULT_SUCCESS, response_code, response_headers, PackedByteArray());
					return true;
				}
				if (body_len < 0) {
					// Chunked transfer is done.
					download_complete = true;
					_request_done(RESULT_SUCCESS, response_code, response_headers, body);
					return true;
				}

				_request_done(RESULT_CHUNKED_BODY_SIZE_MISMATCH, response_code, response_headers, PackedByteArray());
				return true;
				// Request might have been done.
			} else {
				// Did not request yet, do request.

				int size = request_data.size();
				Error err = client->request(method, request_string, headers, size > 0 ? request_data.ptr() : nullptr, size);
				if (err != OK) {
					_request_done(RESULT_CONNECTION_ERROR, 0, PackedStringArray(), PackedByteArray());
					return true;
				}

				request_sent = true;
				return false;
			}
		} break; // Connected: break requests only accepted here.
		case HTTPClient::STATUS_REQUESTING: {
			// Must wait, still requesting.
			client->poll();
			return false;

		} break; // Request in progress.
		case HTTPClient::STATUS_BODY: {
			if (!got_response) {
				bool ret_value;

				if (_handle_response(&ret_value)) {
					return ret_value;
				}

				if (!client->is_response_chunked() && client->get_response_body_length() == 0) {
					_request_done(RESULT_SUCCESS, response_code, response_headers, PackedByteArray());
					return true;
				}

				// No body len (-1) if chunked or no content-length header was provided.
				// Change your webserver configuration if you want body len.
				body_len = client->get_response_body_length();

				if (body_size_limit >= 0 && body_len > body_size_limit) {
					_request_done(RESULT_BODY_SIZE_LIMIT_EXCEEDED, response_code, response_headers, PackedByteArray());
					return true;
				}

				if (!download_to_file.is_empty()) {
					file = FileAccess::open(download_to_file, FileAccess::WRITE);
					if (file.is_null()) {
						_request_done(RESULT_DOWNLOAD_FILE_CANT_OPEN, response_code, response_headers, PackedByteArray());
						return true;
					}
				}
			}

			client->poll();
			if (client->get_status() != HTTPClient::STATUS_BODY) {
				return false;
			}

			PackedByteArray chunk;
			if (decompressor.is_null()) {
				// Chunk can be read directly.
				chunk = client->read_response_body_chunk();
				downloaded.add(chunk.size());
			} else {
				// Chunk is the result of decompression.
				PackedByteArray compressed = client->read_response_body_chunk();
				downloaded.add(compressed.size());

				int pos = 0;
				int left = compressed.size();
				while (left) {
					int w = 0;
					Error err = decompressor->put_partial_data(compressed.ptr() + pos, left, w);
					if (err == OK) {
						PackedByteArray dc;
						dc.resize(decompressor->get_available_bytes());
						err = decompressor->get_data(dc.ptrw(), dc.size());
						chunk.append_array(dc);
					}
					if (err != OK) {
						_request_done(RESULT_BODY_DECOMPRESS_FAILED, response_code, response_headers, PackedByteArray());
						return true;
					}
					// We need this check here because a "zip bomb" could result in a chunk of few kilos decompressing into gigabytes of data.
					if (body_size_limit >= 0 && final_body_size.get() + chunk.size() > body_size_limit) {
						_request_done(RESULT_BODY_SIZE_LIMIT_EXCEEDED, response_code, response_headers, PackedByteArray());
						return true;
					}
					pos += w;
					left -= w;
				}
			}
			final_body_size.add(chunk.size());

			if (body_size_limit >= 0 && final_body_size.get() > body_size_limit) {
				_request_done(RESULT_BODY_SIZE_LIMIT_EXCEEDED, response_code, response_headers, PackedByteArray());
				return true;
			}

			if (chunk.size()) {
				if (file.is_valid()) {
					const uint8_t *r = chunk.ptr();
					file->store_buffer(r, chunk.size());
					if (file->get_error() != OK) {
						_request_done(RESULT_DOWNLOAD_FILE_WRITE_ERROR, response_code, response_headers, PackedByteArray());
						return true;
					}
				} else {
					body.append_array(chunk);
				}
			}

			if (body_len >= 0) {
				if (downloaded.get() == body_len) {
					download_complete = true;
					_request_done(RESULT_SUCCESS, response_code, response_headers, body);
					return true;
				}
			} else if (client->get_status() == HTTPClient::STATUS_DISCONNECTED) {
				// We read till EOF, with no errors. Request is done.
				_request_done(RESULT_SUCCESS, response_code, response_headers, body);
				return true;
			}

			return false;

		} break; // Request resulted in body: break which must be read.
		case HTTPClient::STATUS_CONNECTION_ERROR: {
			_request_done(RESULT_CONNECTION_ERROR, 0, PackedStringArray(), PackedByteArray());
			return true;
		} break;
		case HTTPClient::STATUS_TLS_HANDSHAKE_ERROR: {
			_request_done(RESULT_TLS_HANDSHAKE_ERROR, 0, PackedStringArray(), PackedByteArray());
			return true;
		} break;
	}

	ERR_FAIL_V(false);
}

void HTTPRequester::_request_done(Result p_status, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_data) {
	cancel_request();
	result_status.set(p_status);
}

void HTTPRequester::set_use_threads(bool p_use) {
	ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED);
#ifdef THREADS_ENABLED
	use_threads.set_to(p_use);
#endif
}

bool HTTPRequester::is_using_threads() const {
	return use_threads.is_set();
}

void HTTPRequester::set_accept_gzip(bool p_gzip) {
	accept_gzip = p_gzip;
}

bool HTTPRequester::is_accepting_gzip() const {
	return accept_gzip;
}

void HTTPRequester::set_body_size_limit(int p_bytes) {
	ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED);

	body_size_limit = p_bytes;
}

int HTTPRequester::get_body_size_limit() const {
	return body_size_limit;
}

void HTTPRequester::set_download_file(const String &p_file) {
	ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED);

	download_to_file = p_file;
}

String HTTPRequester::get_download_file() const {
	return download_to_file;
}

void HTTPRequester::set_download_chunk_size(int p_chunk_size) {
	ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED);

	client->set_read_chunk_size(p_chunk_size);
}

int HTTPRequester::get_download_chunk_size() const {
	return client->get_read_chunk_size();
}

HTTPClient::Status HTTPRequester::get_http_client_status() const {
	return client->get_status();
}

void HTTPRequester::set_max_redirects(int p_max) {
	max_redirects = p_max;
}

int HTTPRequester::get_max_redirects() const {
	return max_redirects;
}

int HTTPRequester::get_downloaded_bytes() const {
	return downloaded.get();
}

int HTTPRequester::get_body_size() const {
	return body_len;
}

void HTTPRequester::set_http_proxy(const String &p_host, int p_port) {
	client->set_http_proxy(p_host, p_port);
}

void HTTPRequester::set_https_proxy(const String &p_host, int p_port) {
	client->set_https_proxy(p_host, p_port);
}

void HTTPRequester::set_time_limit(double p_timeout) {
	ERR_FAIL_COND(p_timeout < 0);
	time_limit = p_timeout;
}

double HTTPRequester::get_time_limit() const {
	return time_limit;
}

void HTTPRequester::set_request_timeout(double p_request_timeout) {
	ERR_FAIL_COND(p_request_timeout < 0);
	request_timeout = p_request_timeout;
}

double HTTPRequester::get_request_timeout() const {
	return request_timeout;
}

void HTTPRequester::set_tls_options(const Ref<TLSOptions> &p_options) {
	ERR_FAIL_COND(p_options.is_null() || p_options->is_server());
	tls_options = p_options;
}

HTTPRequester::HTTPRequester() {
	_reset();
	client = Ref<HTTPClient>(HTTPClient::create());
	tls_options = TLSOptions::client();
}

bool HTTPRequester::poll() {
	if (_update_connection()) {
		return true;
	}
	auto current_time = OS::get_singleton()->get_unix_time();
	if (previous_status < HTTPClient::STATUS_BODY && request_timeout > 0 && current_time - current_status_start_time > request_timeout) {
		cancel_request();
		_request_done(RESULT_REQUEST_TIMEOUT, 0, PackedStringArray(), PackedByteArray());
		return true;
	}
	if (time_limit > 0 && current_time - start_time > time_limit) {
		cancel_request();
		_request_done(RESULT_TIMEOUT, 0, PackedStringArray(), PackedByteArray());
		return true;
	}
	return false;
}

double HTTPRequester::get_progress() const {
	if (body_len == 0) {
		return 0;
	}
	return (double)downloaded.get() / (double)body_len;
}

HTTPRequester::Result HTTPRequester::get_result_status() const {
	return result_status.get();
}

void HTTPRequester::_reset() {
	body.clear();
	got_response = false;
	response_code = -1;
	request_sent = false;
	download_complete = false;
	start_time = 0;
	current_status_start_time = 0;
	result_status.set(RESULT_NOT_DONE);
}

Error HTTPRequester::_get_error_from_status(Result p_status, int p_code) {
	switch (p_status) {
		case RESULT_SUCCESS:
			return OK;
		case RESULT_CANCELLED:
			return ERR_SKIP;
		default:
			break;
	}
	if (p_status == RESULT_SUCCESS) {
		return OK;
	}
	if (p_status == RESULT_CANCELLED) {
		return ERR_SKIP;
	}
	switch (p_code) {
		case 404:
			return ERR_FILE_NOT_FOUND;
		case 403:
			return ERR_FILE_NO_PERMISSION;
		case 401:
			return ERR_UNAUTHORIZED;
		case 425:
		case 429:
		case 503:
			return ERR_BUSY;
		case 504:
			return ERR_TIMEOUT;
		default:
			break;
	}
	switch (p_status) {
		case RESULT_REQUEST_TIMEOUT:
		case RESULT_TIMEOUT:
			return ERR_TIMEOUT;
		case RESULT_CANT_RESOLVE:
			return ERR_CANT_RESOLVE;
		case RESULT_CONNECTION_ERROR:
		case RESULT_TLS_HANDSHAKE_ERROR:
		case RESULT_REQUEST_FAILED:
			return ERR_CONNECTION_ERROR;
		default:
			break;
	}
	return ERR_BUG;
}

Error HTTPRequester::_wait_for_request_completion(HTTPRequester *requester, int max_retries, float *p_progress, bool *p_cancelled, int64_t *r_size) {
	String url = requester->full_url;
	Vector<uint8_t> request_data = requester->request_data;
	Vector<String> headers = requester->headers;
	Error ret;
	int max_tries = MAX(max_retries == INT_MAX ? INT_MAX : max_retries + 1, 1);
	for (int retries = 0; retries < max_tries; retries++) {
		ret = OK;
		bool set_rsize = false;
		// Poll until done
		while (!(p_cancelled && *p_cancelled) && !requester->poll()) {
			if (p_cancelled && *p_cancelled) {
				requester->cancel_request();
				requester->result_status.set(RESULT_CANCELLED);
				return ERR_SKIP;
			}
			if (p_progress) {
				*p_progress = requester->get_progress();
			}
			if (!set_rsize && r_size) {
				int64_t body_size = requester->get_body_size();
				if (body_size > 0) {
					*r_size = body_size;
					set_rsize = true;
				}
			}
		}

		// Done
		auto result = requester->result_status.get();
		ret = _get_error_from_status(requester->result_status.get(), requester->response_code);
		// Don't retry if we timed out while waiting to connect or the DNS failed to resolve
		if ((result == RESULT_TIMEOUT || result == RESULT_REQUEST_TIMEOUT) && (requester->previous_status == HTTPClient::STATUS_CONNECTING || requester->previous_status == HTTPClient::STATUS_RESOLVING)) {
			break;
		}
		if (ret != OK && retries + 1 < max_tries) {
			int response_code = requester->response_code;
			// Don't bother retrying if the file doesn't exist or we don't have access to it
			if (response_code == 404 || response_code == 403 || response_code == 401) {
				break;
			}
			if (ret == ERR_BUSY || ret == ERR_TIMEOUT) {
				// backoff by 1 second for each retry
				OS::get_singleton()->delay_usec(1000000 * (retries + 1));
			}
			requester->start_request_raw(url, headers, HTTPClient::METHOD_GET, request_data);
		} else {
			break;
		}
	}
	return ret;
}

Error HTTPRequester::wget_sync(const String &p_url, Vector<uint8_t> &response, double timeout, int retries, const Vector<String> &extra_headers, float *p_progress, bool *p_cancelled) {
	HTTPRequester requester;
	requester.set_time_limit(timeout);
	requester.set_request_timeout(MIN(HTTPRequester::get_default_request_timeout(), timeout));
	auto start_time = OS::get_singleton()->get_unix_time();
	requester.start_request(p_url, extra_headers, HTTPClient::METHOD_GET);
	Error err = _wait_for_request_completion(&requester, retries, p_progress, p_cancelled, nullptr);
#if 0
	auto end_time = OS::get_singleton()->get_unix_time();
	print_line(vformat("wget_sync %s took %s seconds", p_url, String::num_int64(end_time - start_time)));
#endif
	response = requester.body;
	return err;
}

Error HTTPRequester::download_file_sync(const String &p_url, const String &p_output_path, float *p_progress, bool *p_cancelled, int64_t *r_size) {
	Error dir_err = gdre::ensure_dir(p_output_path.get_base_dir());
	if (dir_err) {
		return dir_err;
	}

	HTTPRequester requester;
	requester.set_time_limit(0);
	requester.set_request_timeout(HTTPRequester::get_default_request_timeout());
	requester.set_download_file(p_output_path);
	requester.start_request(p_url);

	return _wait_for_request_completion(&requester, 2, p_progress, p_cancelled, r_size);
}

double HTTPRequester::get_default_request_timeout() {
	return GLOBAL_GET("network/limits/tcp/connect_timeout_seconds").operator double() * 3;
}
