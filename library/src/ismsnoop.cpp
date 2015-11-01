#include "ismsnoop/ismsnoop.h"

#include <fstream>
#include <iostream>
#include <vector>

struct ISMSnoopInstrumentImage
{
	int width;
	int height;
	int depth;
	std::vector<char> bytes;
};

struct ISMSnoopInstrument
{
	ISMSnoopInstrumentImage panel_icon;
	std::string name;
};

const auto MAGIC_BYTE = 341;
//const auto PANEL_ICON_FILE_NAME_LENGTH_BYTE = 349;

template <class T>
T swap_endian(T x)
{
	T result;

	unsigned char *dst = (unsigned char *)(&result);
	unsigned char *src = (unsigned char *)(&x);

	for(unsigned int i = 0; i < sizeof(T); i++)
	{
		dst[i] = src[sizeof(T) - 1 - i];
	}

	return result;
}

static bool looks_like_a_background_image(const std::vector<uint32_t> & chunk, int * file_name_length_idx)
{
	if (chunk[0] == 0
		&& chunk[1] == 0
		&& chunk[2] == 0
		&& chunk[3] == 9
		&& chunk[4] == 2
		&& chunk[5] == 11
		&& chunk[11] == 0
		&& chunk[12] == 0
		&& chunk[13] == 1
		&& chunk[15] == 1)
	{
		*file_name_length_idx = 16;
		return true;
	}

	if (chunk[0] == 0
		&& chunk[1] == 0
		&& chunk[2] == 0
		&& chunk[3] == 0
		&& chunk[4] == 65536
		&& chunk[5] == 65536
		&& chunk[6] == 1
		&& chunk[7] == 11
		&& chunk[8] == 0
		&& chunk[10] == chunk[9]
		&& chunk[11] == chunk[9]
		&& chunk[12] == chunk[9]
		&& chunk[17] == 1)
	{
		*file_name_length_idx = 18;
		return true;
	}

	return false;
}

static bool looks_like_the_info_text(const std::vector<uint32_t> & chunk)
{
	if (chunk[0] == 0
		&& chunk[1] == 0
		&& chunk[2] == 0
		&& chunk[3] == 131072
		&& chunk[4] == 16842752
		&& chunk[5] == 16843009)
	{
		return true;
	}

	return false;
}

ISMSnoopInstrument * ismsnoop_open(const char * path)
{
	std::ifstream ifs(path, std::ios_base::in | std::ios_base::binary);

	if(!ifs.good())
	{
		return nullptr;
	}

	ifs.seekg(0, std::ios::end);

	const auto file_length = ifs.tellg();

	if(file_length < MAGIC_BYTE)
	{
		return nullptr;
	}

	const auto result = new ISMSnoopInstrument();

	ifs.seekg(MAGIC_BYTE, std::ios::beg);

	char magic_byte;

	ifs.get(magic_byte);

	if (magic_byte == 0)
	{
		// no panel icon
		result->panel_icon.width = 0;
		result->panel_icon.height = 0;
		result->panel_icon.depth = 0;

		ifs.seekg(3, std::ios::cur);
	}
	else
	{
		ifs.seekg(7, std::ios::cur);

		char panel_icon_file_name_length;

		ifs.get(panel_icon_file_name_length);

		ifs.seekg(48 + panel_icon_file_name_length, std::ios::cur);

		uint16_t panel_icon_width, panel_icon_height, panel_icon_depth;

		ifs.read((char*)(&panel_icon_width), 2);
		ifs.read((char*)(&panel_icon_height), 2);
		ifs.read((char*)(&panel_icon_depth), 2);

		result->panel_icon.width = panel_icon_width;
		result->panel_icon.height = panel_icon_height;
		result->panel_icon.depth = panel_icon_depth;

		const auto channels = panel_icon_depth / 8;
		const auto panel_icon_size = result->panel_icon.width * result->panel_icon.height * channels;

		for(auto i = 0; i < panel_icon_size; i++)
		{
			char byte;

			ifs.get(byte);

			result->panel_icon.bytes.push_back(byte);
		}
	}

	std::vector<uint32_t> chunk;

	for (int i = 0; i < 19; i++)
	{
		uint32_t x;
		ifs.read((char*)(&x), 4);
		chunk.push_back(x);
	}

	int file_name_length_idx;

	int num_bg_images = 0;

	while (looks_like_a_background_image(chunk, &file_name_length_idx))
	{
		uint32_t file_name_length = chunk[file_name_length_idx];

		if (file_name_length_idx == 16)
		{
			ifs.seekg(37 + file_name_length, std::ios::cur);
		}

		if (file_name_length_idx == 18)
		{
			ifs.seekg(45 + file_name_length, std::ios::cur);
		}

		uint16_t width, height, depth;

		ifs.read((char*)(&width), 2);
		ifs.read((char*)(&height), 2);
		ifs.read((char*)(&depth), 2);

		const auto channels = depth / 8;
		const auto image_size = width * height * channels;

		ifs.seekg(image_size, std::ios::cur);
		ifs.seekg(2, std::ios::cur);

		num_bg_images++;

		chunk.clear();

		for (int i = 0; i < 19; i++)
		{
			uint32_t x;
			ifs.read((char*)(&x), 4);
			chunk.push_back(x);
		}
	}

	if (num_bg_images == 1)
	{
		ifs.seekg(2, std::ios::cur);
		chunk.clear();

		for (int i = 0; i < 10; i++)
		{
			uint32_t x;
			ifs.read((char*)(&x), 4);
			chunk.push_back(x);
		}
	}
	else
	{
		chunk.erase(chunk.begin());
		ifs.seekg(-32, std::ios::cur);
	}

	int name_length_idx;

	if (looks_like_the_info_text(chunk))
	{
		const auto name_length = chunk[9];

		std::vector<char> buffer(name_length);

		ifs.read(buffer.data(), name_length);

		result->name = std::string(buffer.begin(), buffer.end());
	}

	return result;
}

void ismsnoop_close(ISMSnoopInstrument * instrument)
{
	delete instrument;
}

void ismsnoop_get_panel_icon_size(ISMSnoopInstrument * instrument, int * width, int * height, int * depth)
{
	*width = instrument->panel_icon.width;
	*height = instrument->panel_icon.height;
	*depth = instrument->panel_icon.depth;
}

void ismsnoop_get_panel_icon_bytes(ISMSnoopInstrument * instrument, char * dest)
{
	int i = 0;

	for(const auto byte : instrument->panel_icon.bytes)
	{
		dest[i++] = byte;
	}
}

void ismsnoop_get_name(ISMSnoopInstrument * instrument, char * dest, int * length)
{
	if (length)
	{
		*length = instrument->name.size();
	}

	if (dest)
	{
		strncpy(dest, instrument->name.c_str(), instrument->name.size());
		dest[instrument->name.size()] = 0;
	}
}
