
#include "sample_saver.h"
#include "core/os/file_access.h"
#include "core/io/marshalls.h"

Error SampleSaver::save_to_wav(const String &p_path, const Ref<Sample> &sample) {
	if (sample->get_format() == Sample::FORMAT_IMA_ADPCM) {
	 	WARN_PRINTS("Saving IMA_ADPC samples are not supported yet");
	 	return ERR_UNAVAILABLE;
	}

	int sub_chunk_2_size = sample->get_data().size(); //Subchunk2Size = Size of data in bytes

	// Format code
	// 1:PCM format (for 8 or 16 bit)
	// 3:IEEE float format
	int format_code = (sample->get_format() == Sample::FORMAT_IMA_ADPCM) ? 3 : 1;

	int n_channels = sample->is_stereo() ? 2 : 1;

	long sample_rate = sample->get_mix_rate();

	int bits_pr_sample = 0;
	switch (sample->get_format()) {
		case Sample::FORMAT_PCM8: bits_pr_sample = 8; break;
		case Sample::FORMAT_PCM16: bits_pr_sample = 16; break;
		case Sample::FORMAT_IMA_ADPCM: bits_pr_sample = 4; break;
	}

	String file_path = p_path;
	if (!(file_path.substr(file_path.length() - 4, 4) == ".wav")) {
		file_path += ".wav";
	}

	FileAccessRef file = FileAccess::open(file_path, FileAccess::WRITE); //Overrides existing file if present

	ERR_FAIL_COND_V(!file, ERR_FILE_CANT_WRITE);

	// Create WAV Header
	file->store_string("RIFF"); //ChunkID
    if (sample->get_format() == Sample::FORMAT_IMA_ADPCM){
        file->store_32(sub_chunk_2_size + 40); //ChunkSize = 40 + SubChunk2Size (size of entire file minus the 8 bits for this and previous header)
        file->store_string("WAVE"); //Format
        file->store_string("fmt "); //Subchunk1ID
        file->store_32(20); //Subchunk1Size = 16
        file->store_16(format_code); //AudioFormat
        file->store_16(n_channels); //Number of Channels
        file->store_32(sample_rate); //SampleRate
        file->store_32(4055); //ByteRate
        file->store_16(256); //BlockAlign = NumChannels * BytePrSample
        file->store_16(bits_pr_sample); //BitsPerSample
        file->store_16(2); //cb Size
        file->store_16(505); // validBitsPerSample
        file->store_string("fact"); //factSubchunk2ID
        file->store_32(4); //factchunkSize
        file->store_32(sub_chunk_2_size*2); //dwSampleLength
        file->store_string("data"); //Subchunk2ID
        file->store_32(sub_chunk_2_size); //Subchunk2Size

    } else {
        file->store_32(sub_chunk_2_size + 36); //ChunkSize = 36 + SubChunk2Size (size of entire file minus the 8 bits for this and previous header)
        file->store_string("WAVE"); //Format
        file->store_string("fmt "); //Subchunk1ID
        file->store_32(16); //Subchunk1Size = 16
        file->store_16(format_code); //AudioFormat
        file->store_16(n_channels); //Number of Channels
        file->store_32(sample_rate); //SampleRate
        file->store_32(sample_rate * n_channels * (bits_pr_sample/8)); //ByteRate
        file->store_16(n_channels * (bits_pr_sample/8)); //BlockAlign = NumChannels * BytePrSample
        file->store_16(bits_pr_sample); //BitsPerSample
        file->store_string("data"); //Subchunk2ID
        file->store_32(sub_chunk_2_size); //Subchunk2Size

    }


	// Add data
	DVector<uint8_t> data = sample->get_data();
	DVector<uint8_t>::Read read_data = data.read();
	switch (sample->get_format()) {
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
        	for (unsigned int i = 0; i < sub_chunk_2_size; i++) {
				uint8_t data_point = (read_data[i]);
				file->store_8(data_point);
			}
			break;

	}
	file->close();
	return OK;
}
