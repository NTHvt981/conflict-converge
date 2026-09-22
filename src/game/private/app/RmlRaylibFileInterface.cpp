#include "RmlRaylibFileInterface.h"

#include <cstdio>

Rml::FileHandle RmlRaylibFileInterface::Open(const Rml::String& path)
{
	FILE* fp = nullptr;
	fopen_s(&fp, path.c_str(), "rb");
	return reinterpret_cast<Rml::FileHandle>(fp);
}

void RmlRaylibFileInterface::Close(Rml::FileHandle file)
{
	if (FILE* fp = reinterpret_cast<FILE*>(file))
		fclose(fp);
}

size_t RmlRaylibFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file)
{
	if (FILE* fp = reinterpret_cast<FILE*>(file))
		return fread(buffer, 1, size, fp);
	return 0;
}

bool RmlRaylibFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
{
	if (FILE* fp = reinterpret_cast<FILE*>(file))
		return fseek(fp, offset, origin) == 0;
	return false;
}

size_t RmlRaylibFileInterface::Tell(Rml::FileHandle file)
{
	if (FILE* fp = reinterpret_cast<FILE*>(file))
		return static_cast<size_t>(ftell(fp));
	return 0;
}
