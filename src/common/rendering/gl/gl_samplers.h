#ifndef __GL_SAMPLERS_H
#define __GL_SAMPLERS_H

#include "gl/textures/gl_hwtexture.h"
#include "textures/textures.h"

namespace OpenGLRenderer
{

// Sampler constants - these match GZDoom's sampler system
enum
{
	CLAMP_NONE = 0,
	CLAMP_X = 1,
	CLAMP_Y = 2,
	CLAMP_XY = 3,
	CLAMP_NOFILTER = 4,
	CLAMP_NOFILTER_X = 5,
	CLAMP_NOFILTER_Y = 6,
	CLAMP_NOFILTER_XY = 7,
	CLAMP_XY_NOMIP = 8,
	CLAMP_CAMTEX = 9,
	NUMSAMPLERS = 10
};

class FSamplerManager
{
	unsigned int mSamplers[NUMSAMPLERS];

	void UnbindAll();

public:

	FSamplerManager();
	~FSamplerManager();

	uint8_t Bind(int texunit, int num, int lastval);
	void SetTextureFilterMode();


};

}
#endif
