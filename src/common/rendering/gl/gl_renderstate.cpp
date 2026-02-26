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
/*
** gl_renderstate.cpp
** Render state maintenance
**
*/

#include "gl/system/gl_system.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"  // For FStateVec3
#include "gl_renderstate.h"
#include "gl_buffers.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/textures/gl_material.h"  // zandronum's FMaterial
// Don't include hw_material.h - it conflicts with zandronum's FMaterial
// #include "common/textures/hw_material.h"
#include "r_data/renderstyle.h"
#include "gl/renderer/gl_colormap.h"
#include "c_console.h"  // For Printf
#include "c_dispatch.h"  // For CCMD

namespace OpenGLRenderer
{

FGLRenderState gl_RenderState;

static VSMatrix identityMatrix(1);

static void matrixToGL(const VSMatrix &mat, int loc)
{
	glUniformMatrix4fv(loc, 1, false, (float*)&mat);
}

//==========================================================================
//
// This only gets called once upon setup.
// With OpenGL the state is persistent and cannot be cleared, once set up.
//
//==========================================================================

void FGLRenderState::Reset()
{
	static bool firstReset = true;
	if (firstReset)
	{
		firstReset = false;
		// Print a message to confirm new OpenGL architecture is active
		Printf("OpenGL: New render state architecture active (FGLRenderState)\n");
	}

	FRenderState::Reset();
	mVertexBuffer = mCurrentVertexBuffer = nullptr;
	mGlossiness = 0.0f;
	mSpecularLevel = 0.0f;
	mShaderTimer = 0.0f;

	stRenderStyle = DefaultRenderStyle();
	stSrcBlend = stDstBlend = -1;
	stBlendEquation = -1;
	stAlphaTest = 0;
	mLastDepthClamp = true;

	mEffectState = 0;
	activeShader = nullptr;

	mCurrentVertexBuffer = nullptr;
	mCurrentVertexOffsets[0] = mVertexOffsets[0] = 0;
	mCurrentVertexOffsets[1] = mVertexOffsets[1] = 0;
	mCurrentIndexBuffer = nullptr;
}

//==========================================================================
//
// Apply shader settings
//
//==========================================================================

bool FGLRenderState::ApplyShader()
{
	// Integrate with zandronum's shader system
	if (!GLRenderer || !GLRenderer->mShaderManager)
	{
		return false;
	}

	FShader *shader = nullptr;

	// Handle special effects
	if (mSpecialEffect > EFF_NONE)
	{
		shader = GLRenderer->mShaderManager->BindEffect(mSpecialEffect);
		if (shader)
		{
			activeShader = shader;
			GLRenderer->mShaderManager->SetActiveShader(shader);
		}
	}
	else
	{
		// Get shader container based on effect state
		FShaderContainer *container = GLRenderer->mShaderManager->Get(mTextureEnabled ? mEffectState : 4);
		if (container)
		{
			// Determine colormap state - simplified, will need to track this properly
			int cm = CM_DEFAULT;
			if (mColorMapSpecial > 0)
			{
				cm = mColorMapSpecial;
			}

			// Bind shader with appropriate parameters
			// Use mLightIndex >= 0 as indicator for light enabled
			bool lightEnabled = (mLightIndex >= 0);
			shader = container->Bind(cm, mGlowEnabled, mShaderTimer, lightEnabled);
			if (shader)
			{
				activeShader = shader;
				GLRenderer->mShaderManager->SetActiveShader(shader);
			}
		}
	}

	if (shader)
	{
		// Set shader uniforms using zandronum's FShader interface
		int fogset = 0;
		if (mFogEnabled)
		{
			// Get fog mode from CVAR (defined in gl_cvars.h)
			if ((GetFogColor() & 0xffffff) == 0)
			{
				fogset = gl_fogmode;
			}
			else
			{
				fogset = -gl_fogmode;
			}
		}

		// Update fog uniform if changed
		if (fogset != shader->currentfogenabled && shader->fogenabled_index >= 0)
		{
			glUniform1i(shader->fogenabled_index, (shader->currentfogenabled = fogset));
		}

		// Update texture mode uniform if changed
		int textureMode = GetTextureModeAndFlags(mTempTM);
		if (textureMode != shader->currenttexturemode && shader->texturemode_index >= 0)
		{
			glUniform1i(shader->texturemode_index, (shader->currenttexturemode = textureMode));
		}

		// Update camera position
		if (shader->camerapos_index >= 0)
		{
			// Get camera position from GLRenderer if available
			FStateVec3 camPos;
			if (GLRenderer)
			{
				camPos.Set(GLRenderer->mCameraPos.X, GLRenderer->mCameraPos.Y, GLRenderer->mCameraPos.Z);
			}
			else
			{
				camPos.Set(0.0f, 0.0f, 0.0f);
			}
			if (camPos.Update(&shader->currentcamerapos))
			{
				glUniform3fv(shader->camerapos_index, 1, camPos.vec);
			}
		}

		// Update fog params
		if (shader->lightparms_index >= 0)
		{
			const float LOG2E = 1.442692f;
			float fogDensity = mLightParms[2] * (64000.f / -LOG2E);
			glVertexAttrib4f(VATTR_FOGPARAMS, mLightParms[0], mLightParms[1], mLightParms[2], 0);
		}

		// Update fog color
		PalEntry fogColor = GetFogColor();
		if (fogColor != shader->currentfogcolor && shader->fogcolor_index >= 0)
		{
			shader->currentfogcolor = fogColor;
			glUniform4f(shader->fogcolor_index, fogColor.r/255.f, fogColor.g/255.f, 
						fogColor.b/255.f, 0);
		}

		// Update glow params
		if (mGlowEnabled && shader->glowtopcolor_index >= 0 && shader->glowbottomcolor_index >= 0)
		{
			glUniform4fv(shader->glowtopcolor_index, 1, &mStreamData.uGlowTopColor.X);
			glUniform4fv(shader->glowbottomcolor_index, 1, &mStreamData.uGlowBottomColor.X);
		}

		// Update dynamic light color (only for lightmode 8)
		bool lightEnabled = (mLightIndex >= 0);
		if (lightEnabled && shader->dlightcolor_index >= 0 && gl_lightmode == 8)
		{
			glUniform3fv(shader->dlightcolor_index, 1, &mStreamData.uDynLightColor.X);
		}
	}
	else
	{
		// No shader active
		activeShader = nullptr;
		GLRenderer->mShaderManager->SetActiveShader(nullptr);
	}

	// Set vertex attributes
	glVertexAttrib4fv(VATTR_COLOR, &mStreamData.uVertexColor.X);
	glVertexAttrib4fv(VATTR_NORMAL, &mStreamData.uVertexNormal.X);

	return shader != nullptr;
}

//==========================================================================
//
// Apply State
//
//==========================================================================

void FGLRenderState::ApplyState()
{
	// Compare render styles using AsDWORD to avoid ambiguous operator!= overload
	if (mRenderStyle.AsDWORD != stRenderStyle.AsDWORD)
	{
		ApplyBlendMode();
		stRenderStyle = mRenderStyle;
	}

	if (mSplitEnabled != stSplitEnabled)
	{
		if (mSplitEnabled)
		{
			glEnable(GL_CLIP_DISTANCE3);
			glEnable(GL_CLIP_DISTANCE4);
		}
		else
		{
			glDisable(GL_CLIP_DISTANCE3);
			glDisable(GL_CLIP_DISTANCE4);
		}
		stSplitEnabled = mSplitEnabled;
	}

	if (mMaterial.mChanged)
	{
		ApplyMaterial(reinterpret_cast<::FMaterial*>(mMaterial.mMaterial), mMaterial.mClampMode, mMaterial.mTranslation, mMaterial.mOverrideShader);
		mMaterial.mChanged = false;
	}

	if (mBias.mChanged)
	{
		if (mBias.mFactor == 0 && mBias.mUnits == 0)
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		else
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
		}
		glPolygonOffset(mBias.mFactor, mBias.mUnits);
		mBias.mChanged = false;
	}
}

void FGLRenderState::ApplyBuffers()
{
	if (mVertexBuffer != mCurrentVertexBuffer || mVertexOffsets[0] != mCurrentVertexOffsets[0] || mVertexOffsets[1] != mCurrentVertexOffsets[1])
	{
		assert(mVertexBuffer != nullptr);
		static_cast<GLVertexBuffer*>(mVertexBuffer)->Bind(mVertexOffsets);
		mCurrentVertexBuffer = mVertexBuffer;
		mCurrentVertexOffsets[0] = mVertexOffsets[0];
		mCurrentVertexOffsets[1] = mVertexOffsets[1];
	}
	if (mIndexBuffer != mCurrentIndexBuffer)
	{
		if (mIndexBuffer) static_cast<GLIndexBuffer*>(mIndexBuffer)->Bind();
		mCurrentIndexBuffer = mIndexBuffer;
	}
}

void FGLRenderState::Apply()
{
	ApplyState();
	ApplyBuffers();
	ApplyShader();
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

void FGLRenderState::ApplyMaterial(::FMaterial *mat, int clampmode, int translation, int overrideshader)
{
	// Integrate with zandronum's existing FMaterial system
	if (!mat) return;

	// Avoid rebinding the same texture multiple times.
	if (mat == lastMaterial && lastClamp == clampmode && translation == lastTranslation) return;
	lastMaterial = mat;
	lastClamp = clampmode;
	lastTranslation = translation;

	// Use zandronum's existing FMaterial::Bind() method
	// Determine colormap - simplified, will need proper tracking
	int cm = CM_DEFAULT;
	if (mColorMapSpecial > 0)
	{
		cm = mColorMapSpecial;
	}

	// Bind the material using zandronum's existing system
	mat->Bind(cm, clampmode, translation, overrideshader);

	// Update effect state from material's shader index
	// mShaderIndex is private, but we can get it from the shader manager or use overrideshader
	mEffectState = overrideshader >= 0 ? overrideshader : 0;  // Simplified - will need proper shader index tracking
	
	// Update texture mode based on material properties
	// mat->tex is public in zandronum's FMaterial
	if (mat->tex && mat->tex->bHasCanvas)
	{
		mTempTM = TM_OPAQUE;
	}
	else
	{
		mTempTM = TM_NORMAL;
	}
}

//==========================================================================
//
// Apply blend mode from RenderStyle
//
//==========================================================================

void FGLRenderState::ApplyBlendMode()
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1 };

	int srcblend = blendstyles[mRenderStyle.SrcAlpha%STYLEALPHA_MAX];
	int dstblend = blendstyles[mRenderStyle.DestAlpha%STYLEALPHA_MAX];
	int blendequation = renderops[mRenderStyle.BlendOp & 15];

	if (blendequation == -1)	// This was a fuzz style.
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	stSrcBlend = srcblend;
	stDstBlend = dstblend;
	glBlendFunc(srcblend, dstblend);

	stBlendEquation = blendequation;
	glBlendEquation(blendequation);
}

//==========================================================================
//
// API dependent draw calls
//
//==========================================================================

static int dt2gl[] = { GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP };

void FGLRenderState::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply();
	}
	glDrawArrays(dt2gl[dt], index, count);
}

void FGLRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply();
	}
	glDrawElements(dt2gl[dt], count, GL_UNSIGNED_INT, (void*)(intptr_t)(index * sizeof(uint32_t)));
}

void FGLRenderState::SetDepthMask(bool on)
{
	glDepthMask(on);
}

void FGLRenderState::SetDepthFunc(int func)
{
	static int df2gl[] = { GL_LESS, GL_LEQUAL, GL_ALWAYS };
	glDepthFunc(df2gl[func]);
}

void FGLRenderState::SetDepthRange(float min, float max)
{
	glDepthRange(min, max);
}

void FGLRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
	glColorMask(r, g, b, a);
}

void FGLRenderState::SetStencil(int offs, int op, int flags)
{
	static int op2gl[] = { GL_KEEP, GL_INCR, GL_DECR };

	// Get stencil value - simplified for zandronum
	int stencilValue = 0;
	// if (screen) stencilValue = screen->stencilValue;
	
	glStencilFunc(GL_EQUAL, stencilValue + offs, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, op2gl[op]);

	if (flags != -1)
	{
		bool cmon = !(flags & SF_ColorMaskOff);
		glColorMask(cmon, cmon, cmon, cmon);
		glDepthMask(!(flags & SF_DepthMaskOff));
	}
}

void FGLRenderState::ToggleState(int state, bool on)
{
	if (on)
	{
		glEnable(state);
	}
	else
	{
		glDisable(state);
	}
}

void FGLRenderState::SetCulling(int mode)
{
	if (mode != Cull_None)
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(mode == Cull_CCW ? GL_CCW : GL_CW);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}
}

void FGLRenderState::EnableClipDistance(int num, bool state)
{
	// Update the viewpoint-related clip plane setting.
	// RFL_NO_CLIP_PLANES not available in zandronum yet
	ToggleState(GL_CLIP_DISTANCE0 + num, state);
}

void FGLRenderState::Clear(int targets)
{
	// This always clears to default values.
	int gltarget = 0;
	if (targets & CT_Depth)
	{
		gltarget |= GL_DEPTH_BUFFER_BIT;
		glClearDepth(1);
	}
	if (targets & CT_Stencil)
	{
		gltarget |= GL_STENCIL_BUFFER_BIT;
		glClearStencil(0);
	}
	if (targets & CT_Color)
	{
		gltarget |= GL_COLOR_BUFFER_BIT;
		// Use default clear color - simplified for zandronum
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	glClear(gltarget);
}

void FGLRenderState::EnableStencil(bool on)
{
	ToggleState(GL_STENCIL_TEST, on);
}

void FGLRenderState::SetScissor(int x, int y, int w, int h)
{
	if (w > -1)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}
}

void FGLRenderState::SetViewport(int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
}

void FGLRenderState::EnableDepthTest(bool on)
{
	ToggleState(GL_DEPTH_TEST, on);
}

void FGLRenderState::EnableMultisampling(bool on)
{
	ToggleState(GL_MULTISAMPLE, on);
}

void FGLRenderState::EnableLineSmooth(bool on)
{
	ToggleState(GL_LINE_SMOOTH, on);
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderState::ClearScreen()
{
	bool multi = !!glIsEnabled(GL_MULTISAMPLE);

	// Simplified clear screen - will need to be expanded
	SetColor(0, 0, 0);
	Apply();

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);

	// Draw fullscreen quad - simplified
	// This will need to use the proper vertex buffer
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glEnable(GL_DEPTH_TEST);
	if (multi) glEnable(GL_MULTISAMPLE);
}

//==========================================================================
//
// Below are less frequently altered state settings which do not get
// buffered by the state object, but set directly instead.
//
//==========================================================================

bool FGLRenderState::SetDepthClamp(bool on)
{
	bool res = mLastDepthClamp;
	if (!on) glDisable(GL_DEPTH_CLAMP);
	else glEnable(GL_DEPTH_CLAMP);
	mLastDepthClamp = on;
	return res;
}

}  // namespace OpenGLRenderer

// Implementation of SetMaterial(FTexture*) from base class - must be outside namespace
namespace HardwareRenderer
{
	void FRenderState::SetMaterial(FTexture* tex, int upscalemask, int scaleflags, int clampmode, int translation, int overrideshader)
	{
		// Compatibility wrapper for zandronum's FTexture
		// Use zandronum's existing FMaterial::ValidateTexture (takes only one argument)
		// Use ::FMaterial to refer to zandronum's FMaterial from gl/textures/gl_material.h
		::FMaterial *mat = ::FMaterial::ValidateTexture(tex);
		if (mat)
		{
			SetMaterial(reinterpret_cast<FMaterial*>(mat), clampmode, translation, overrideshader);
		}
	}
}

//==========================================================================
//
// Console command to verify new OpenGL architecture
//
//==========================================================================

CCMD(gl_renderstate_info)
{
	Printf("OpenGL Render State Architecture:\n");
	Printf("  New architecture: Active (FGLRenderState from common/rendering/gl/)\n");
	Printf("  Old architecture: Compatibility layer (delegates to new)\n");
	Printf("  Render state instance: OpenGLRenderer::gl_RenderState\n");
	Printf("  Base class: HardwareRenderer::FRenderState\n");
}
