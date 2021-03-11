
#include "sample_saver.h"
#include "core/os/file_access.h"
#include "core/io/marshalls.h"

Error SampleSaver::save_to_wav(const String &p_path) {
	if (get_format() == Sample::FORMAT_IMA_ADPCM) {
		WARN_PRINTS("Saving IMA_ADPC samples are not supported yet");
		return ERR_UNAVAILABLE;
	}

	int sub_chunk_2_size = get_data().size(); //Subchunk2Size = Size of data in bytes

	// Format code
	// 1:PCM format (for 8 or 16 bit)
	// 3:IEEE float format
	int format_code = (get_format() == FORMAT_IMA_ADPCM) ? 3 : 1;

	int n_channels = is_stereo() ? 2 : 1;

	long sample_rate = get_mix_rate();

	int byte_pr_sample = 0;
	switch (get_format()) {
		case Sample::FORMAT_PCM8: byte_pr_sample = 1; break;
		case Sample::FORMAT_PCM16: byte_pr_sample = 2; break;
		case Sample::FORMAT_IMA_ADPCM: byte_pr_sample = 4; break;
	}

	String file_path = p_path;
	if (!(file_path.substr(file_path.length() - 4, 4) == ".wav")) {
		file_path += ".wav";
	}

	FileAccessRef file = FileAccess::open(file_path, FileAccess::WRITE); //Overrides existing file if present

	ERR_FAIL_COND_V(!file, ERR_FILE_CANT_WRITE);

	// Create WAV Header
	file->store_string("RIFF"); //ChunkID
	file->store_32(sub_chunk_2_size + 36); //ChunkSize = 36 + SubChunk2Size (size of entire file minus the 8 bits for this and previous header)
	file->store_string("WAVE"); //Format
	file->store_string("fmt "); //Subchunk1ID
	file->store_32(16); //Subchunk1Size = 16
	file->store_16(format_code); //AudioFormat
	file->store_16(n_channels); //Number of Channels
	file->store_32(sample_rate); //SampleRate
	file->store_32(sample_rate * n_channels * byte_pr_sample); //ByteRate
	file->store_16(n_channels * byte_pr_sample); //BlockAlign = NumChannels * BytePrSample
	file->store_16(byte_pr_sample * 8); //BitsPerSample
	file->store_string("data"); //Subchunk2ID
	file->store_32(sub_chunk_2_size); //Subchunk2Size

	// Add data
	DVector<uint8_t> data = get_data();
	DVector<uint8_t>::Read read_data = data.read();
	switch (get_format()) {
		case Sample::FORMAT_PCM8:
			for (unsigned int i = 0; i < sub_chunk_2_size; i++) {
				uint8_t data_point = (read_data[i] + 128);
				file->store_8(data_point);
			}
			break;
		case Sample::FORMAT_PCM16:
			for (unsigned int i = 0; i < sub_chunk_2_size / 2; i++) {
				uint16_t data_point = decode_uint16(&read_data[i * 2]);
				file->store_16(data_point);
			}
			break;
		case Sample::FORMAT_IMA_ADPCM:
			//Unimplemented
			break;
	}
	file->close();
	return OK;
}
