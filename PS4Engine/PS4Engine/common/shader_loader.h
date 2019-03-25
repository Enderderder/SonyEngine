/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2013 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SHADER_LOADER_H_
#define _SHADER_LOADER_H_

#include "../api_gnm/toolkit/shader_loader.h"

bool readFileContents(void *data, const size_t size, FILE *fp)
{
	if( !fp || !data )
		return false;

	uint8_t *address = static_cast<uint8_t*>(data);

	size_t bytesRead, totalBytesRead = 0;
	while( totalBytesRead < size )
	{
		bytesRead = fread(address, 1, size - totalBytesRead, fp);
		if( !bytesRead )
		{
			return false;
		}

		totalBytesRead += bytesRead;
	}

	return true;
}

bool acquireFileContents(void*& pointer, uint32_t& bytes, const char* filename)
{
	pointer = NULL;
	bytes = 0;

	if( !filename )
		return false;

	bool retValue = false;

	FILE *fp = fopen(filename, "rb");
	if( fp )
	{
		if( !fseek(fp, 0, SEEK_END) )
		{
			bytes = ftell(fp);
			if( !fseek(fp, 0, SEEK_SET) )
			{
				pointer = malloc(bytes);
				if( pointer )
				{
					if( readFileContents(pointer, bytes, fp) )
					{
						retValue = true;
					}
					else
					{
						free(pointer);
					}
				}
			}
		}

		fclose(fp);
	}

	if( !retValue )
	{
		pointer = NULL;
		bytes = 0;
	}

	return retValue;
}

template<typename tShader> struct BindEmbeddedShader {};
template<> struct BindEmbeddedShader<sce::Gnmx::VsShader> { sce::Gnmx::Toolkit::EmbeddedVsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::PsShader> { sce::Gnmx::Toolkit::EmbeddedPsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::EsShader> { sce::Gnmx::Toolkit::EmbeddedEsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::GsShader> { sce::Gnmx::Toolkit::EmbeddedGsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::LsShader> { sce::Gnmx::Toolkit::EmbeddedLsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::HsShader> { sce::Gnmx::Toolkit::EmbeddedHsShader embedded; };
template<> struct BindEmbeddedShader<sce::Gnmx::CsShader> { sce::Gnmx::Toolkit::EmbeddedCsShader embedded; };

template<typename tShader> tShader* loadShaderFromFile(const char *path, sce::Gnmx::Toolkit::Allocators &alloc)
{
	BindEmbeddedShader<tShader> binder;
	memset(&binder, 0, sizeof(binder));

	void *binary;
	uint32_t size;
	if( acquireFileContents(binary, size, path) )
	{
		binder.embedded.m_source = static_cast<uint32_t*>(binary);
		binder.embedded.initializeWithAllocators(&alloc);
		free(binary);
	}

	return binder.embedded.m_shader;
}

template<typename tShader> void releaseShader(tShader *shader, sce::Gnmx::Toolkit::Allocators &alloc)
{
	if( shader )
	{
		alloc.m_garlic.release(shader->getBaseAddress());
		alloc.m_onion.release(shader);
	}
}

template<> void releaseShader(sce::Gnmx::GsShader *shader, sce::Gnmx::Toolkit::Allocators &alloc)
{
	if( shader )
	{
		alloc.m_garlic.release(shader->getBaseAddress());
		alloc.m_garlic.release(shader->getCopyShader()->getBaseAddress());
		alloc.m_onion.release(shader);
	}
}

#endif
