#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures/textures.h"

struct FRemapTable;
class FHardwareTexture;

struct MaterialLayerInfo
{
	FTexture* layerTexture;
	int scaleFlags;
	int clampflags;
};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	private:
	TArray<MaterialLayerInfo> mTextureLayers; // the only layers allowed to scale are the brightmap and the glowmap.
	int mShaderIndex;
	int mLayerFlags = 0;
	int mScaleFlags;

public:
	static void SetLayerCallback(FHardwareTexture* (*layercallback)(int layer, int translation));

	FTexture *sourcetex;	// the owning texture. 

	FMaterial(FTexture *tex, int scaleflags);
	virtual ~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	int GetShaderIndex() const { return mShaderIndex; }
	int GetScaleFlags() const { return mScaleFlags; }
	virtual void DeleteDescriptors() { }
	FVector2 GetDetailScale() const
	{
		// Return default detail scale if not available
		return FVector2(1.0f, 1.0f);
	}

	FTexture* Source() const
	{
		return sourcetex;
	}

	void ClearLayers()
	{
		mTextureLayers.Resize(1);
	}

	void AddTextureLayer(FTexture *tex, bool allowscale)
	{
		// CTF_Upscale constant - simplified for zandronum
		const int CTF_Upscale = 1;
		mTextureLayers.Push({ tex, allowscale ? CTF_Upscale : 0, 0 });
	}

	int NumLayers() const
	{
		return mTextureLayers.Size();
	}

	FHardwareTexture *GetLayer(int i, int translation, MaterialLayerInfo **pLayer = nullptr) const;


	static FMaterial *ValidateTexture(FTexture * tex, int scaleflags, bool create = true);
	const TArray<MaterialLayerInfo> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

// Compatibility typedef for FGameTexture
typedef FTexture FGameTexture;

#endif
