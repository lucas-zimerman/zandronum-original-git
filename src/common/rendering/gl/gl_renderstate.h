// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2009-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#ifndef __COMMON_GL_RENDERSTATE_H
#define __COMMON_GL_RENDERSTATE_H

#include <algorithm>
#include <string.h>
#include "gl/system/gl_system.h"  // This includes OpenGL headers (GLenum, glDrawBuffers, etc.)
#include "vectors.h"  // matrix.h is included via vectors.h in zandronum
// VSMatrix is likely defined in gl/system/gl_interface.h or needs to be forward declared
// Forward declare VSMatrix if not found
#ifndef VSMatrix
class VSMatrix;
#endif
#include "common/rendering/hwrenderer/data/hw_renderstate.h"
// Don't include hw_material.h - it conflicts with zandronum's FMaterial
// #include "common/textures/hw_material.h"
#include "c_cvars.h"

// Forward declaration to avoid circular dependency
class FShader;  // Defined in gl/shaders/gl_shader.h

namespace OpenGLRenderer
{

// Forward declarations
struct HWSectorPlane;
// FShader is defined in gl/shaders/gl_shader.h

class FGLRenderState final : public HardwareRenderer::FRenderState
{
	uint8_t mLastDepthClamp : 1;

	float mGlossiness, mSpecularLevel;
	float mShaderTimer;

	int mEffectState;
	int mTempTM = TM_NORMAL;

	FRenderStyle stRenderStyle;
	int stSrcBlend, stDstBlend;
	bool stAlphaTest;
	bool stSplitEnabled;
	int stBlendEquation;

	FShader *activeShader;  // zandronum's FShader type

	int mNumDrawBuffers = 1;

	bool ApplyShader();
	void ApplyState();

	// Texture binding state
	::FMaterial *lastMaterial = nullptr;  // zandronum's FMaterial
	int lastClamp = 0;
	int lastTranslation = 0;
	int maxBoundMaterial = -1;
	size_t mLastMappedLightIndex = SIZE_MAX;
	size_t mLastMappedBoneIndexBase = SIZE_MAX;

	IVertexBuffer *mCurrentVertexBuffer;
	int mCurrentVertexOffsets[2];	// one per binding point
	IIndexBuffer *mCurrentIndexBuffer;


public:

	FGLRenderState()
	{
		Reset();
	}

	void Reset();

	void ClearLastMaterial()
	{
		lastMaterial = nullptr;
	}

	void ApplyMaterial(::FMaterial *mat, int clampmode, int translation, int overrideshader);

	void Apply();
	void ApplyBuffers();
	void ApplyBlendMode();

	void ResetVertexBuffer()
	{
		// forces rebinding with the next 'apply' call.
		mCurrentVertexBuffer = nullptr;
		mCurrentIndexBuffer = nullptr;
	}

	void SetSpecular(float glossiness, float specularLevel)
	{
		mGlossiness = glossiness;
		mSpecularLevel = specularLevel;
	}

	void EnableDrawBuffers(int count, bool apply = false) override
	{
		count = (count < 3) ? count : 3;  // Use ternary instead of min to avoid std::min dependency
		if (mNumDrawBuffers != count)
		{
			static GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(count, buffers);
			mNumDrawBuffers = count;
		}
		if (apply) Apply();
	}

	void ToggleState(int state, bool on);

	void ClearScreen() override;
	void Draw(int dt, int index, int count, bool apply = true) override;
	void DrawIndexed(int dt, int index, int count, bool apply = true) override;

	bool SetDepthClamp(bool on) override;
	void SetDepthMask(bool on) override;
	void SetDepthFunc(int func) override;
	void SetDepthRange(float min, float max) override;
	void SetColorMask(bool r, bool g, bool b, bool a) override;
	void SetStencil(int offs, int op, int flags) override;
	void SetCulling(int mode) override;
	void EnableClipDistance(int num, bool state) override;
	void Clear(int targets) override;
	void EnableStencil(bool on) override;
	void SetScissor(int x, int y, int w, int h) override;
	void SetViewport(int x, int y, int w, int h) override;
	void EnableDepthTest(bool on) override;
	void EnableMultisampling(bool on) override;
	void EnableLineSmooth(bool on) override;


};

extern FGLRenderState gl_RenderState;

}

#endif // __COMMON_GL_RENDERSTATE_H
