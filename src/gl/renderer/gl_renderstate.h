#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "c_cvars.h"
#include "r_data/renderstyle.h"

// Forward declaration to avoid circular dependency
namespace OpenGLRenderer
{
	class FGLRenderState;
}

EXTERN_CVAR(Bool, gl_direct_state_change)

struct FStateAttr
{
	static int ChangeCounter;
	int mLastChange;

	FStateAttr()
	{
		mLastChange = -1;
	}

	bool operator == (const FStateAttr &other)
	{
		return mLastChange == other.mLastChange;
	}

	bool operator != (const FStateAttr &other)
	{
		return mLastChange != other.mLastChange;
	}

};

struct FStateVec3 : public FStateAttr
{
	float vec[3];

	bool Update(FStateVec3 *other)
	{
		if (mLastChange != other->mLastChange)
		{
			*this = *other;
			return true;
		}
		return false;
	}

	void Set(float x, float y, float z)
	{
		vec[0] = x;
		vec[1] = z;
		vec[2] = y;
		mLastChange = ++ChangeCounter;
	}
};

struct FStateVec4 : public FStateAttr
{
	float vec[4];

	bool Update(FStateVec4 *other)
	{
		if (mLastChange != other->mLastChange)
		{
			*this = *other;
			return true;
		}
		return false;
	}

	void Set(float r, float g, float b, float a)
	{
		vec[0] = r;
		vec[1] = g;
		vec[2] = b;
		vec[3] = a;
		mLastChange = ++ChangeCounter;
	}
};


enum EEffect
{
	EFF_NONE,
	EFF_FOGBOUNDARY,
	EFF_SPHEREMAP,
};

class FRenderState
{
	// Keep old members for compatibility
	// Note: New FGLRenderState architecture is available in common/rendering/gl/
	// This class maintains backward compatibility with existing zandronum code
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mLightEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	float mDynLight[3];
	float mLightParms[2];
	int mNumLights[3];
	float *mLightData;
	int mSrcBlend, mDstBlend;
	int mAlphaFunc;
	float mAlphaThreshold;
	bool mAlphaTest;
	int mBlendEquation;
	bool m2D;

	FStateVec3 mCameraPos;
	FStateVec4 mGlowTop, mGlowBottom;
	PalEntry mFogColor;
	float mFogDensity;

	int mEffectState;
	int mColormapState;
	float mWarpTime;

	int glSrcBlend, glDstBlend;
	int glAlphaFunc;
	float glAlphaThreshold;
	bool glAlphaTest;
	int glBlendEquation;

	bool ffTextureEnabled;
	bool ffFogEnabled;
	int ffTextureMode;
	int ffSpecialEffect;
	PalEntry ffFogColor;
	float ffFogDensity;

	bool ApplyShader();
	
	// Get reference to new render state - implementation in .cpp file
	OpenGLRenderer::FGLRenderState& GetNewState();

public:
	FRenderState()
	{
		Reset();
	}

	void Reset();

	int SetupShader(bool cameratexture, int &shaderindex, int &cm, float warptime);
	void Apply(bool forcenoshader = false);

	void SetTextureMode(int mode);
	void EnableTexture(bool on);
	void EnableFog(bool on);
	void SetEffect(int eff);
	void EnableGlow(bool on);
	void EnableLight(bool on)
	{
		mLightEnabled = on;
		// Note: mLightEnabled is tracked but not directly exposed in new render state
		// It's used for shader selection in ApplyShader()
	}
	void EnableBrightmap(bool on);
	void SetCameraPos(float x, float y, float z);
	void SetGlowParams(float *t, float *b);
	void SetDynLight(float r,float g, float b);
	void SetFog(PalEntry c, float d);
	void SetLightParms(float f, float d);
	void SetLights(int *numlights, float *lightdata)
	{
		mNumLights[0] = numlights[0];
		mNumLights[1] = numlights[1];
		mNumLights[2] = numlights[2];
		mLightData = lightdata;	// caution: the data must be preserved by the caller until the 'apply' call!
		// Note: Light data is handled differently in new architecture
		// This will need to be adapted when migrating to new system
	}
	void SetFixedColormap(int cm);

	PalEntry GetFogColor() const
	{
		return mFogColor;
	}

	void BlendFunc(int src, int dst)
	{
		if (!gl_direct_state_change)
		{
			mSrcBlend = src;
			mDstBlend = dst;
		}
		else
		{
			glBlendFunc(src, dst);
		}
	}

	void AlphaFunc(int func, float thresh)
	{
		if (!gl_direct_state_change)
		{
			mAlphaFunc = func;
			mAlphaThreshold = thresh;
		}
		else
		{
			::glAlphaFunc(func, thresh);
		}
	}

	void EnableAlphaTest(bool on)
	{
		if (!gl_direct_state_change)
		{
			mAlphaTest = on;
		}
		else
		{
			if (on) glEnable(GL_ALPHA_TEST);
			else glDisable(GL_ALPHA_TEST);
		}
	}

	void BlendEquation(int eq)
	{
		if (!gl_direct_state_change)
		{
			mBlendEquation = eq;
		}
		else
		{
			::glBlendEquation(eq);
		}
	}

	void Set2DMode(bool on)
	{
		m2D = on;
	}
};

extern FRenderState gl_RenderState;

#endif
