// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Engine/Texture2D.h"

namespace
{
	// UTexture2D::GetSizeX()/GetSizeY() return the *platform* mip size, which in the
	// editor is the default placeholder texture (~32x32) whenever the real texture
	// hasn't finished building/streaming — the engine itself calls that value "garbage"
	// (Texture2D.cpp). Use the authored import size instead; it is stable and matches
	// what the Texture Editor shows.
	FIntPoint AuthoredTextureSize(const UTexture2D* Tex)
	{
		const FIntPoint Imported = Tex->GetImportedSize();
		if (Imported.X > 0 && Imported.Y > 0)
			return Imported;
		return FIntPoint(Tex->GetSizeX(), Tex->GetSizeY()); // fallback if source unavailable
	}

	// The size the game actually loads: authored size clamped by the asset's Max Texture
	// Size. A 4096 source with Max Texture Size 1024 loads as 1024, so cost/VRAM checks
	// should judge it by this, not by the authored 4096.
	FIntPoint EffectiveTextureSize(const UTexture2D* Tex)
	{
		FIntPoint Size = AuthoredTextureSize(Tex);
		const int32 MaxSize = Tex->MaxTextureSize;
		if (MaxSize > 0)
		{
			while (Size.X > MaxSize || Size.Y > MaxSize)
			{
				Size.X = FMath::Max(1, Size.X >> 1);
				Size.Y = FMath::Max(1, Size.Y >> 1);
			}
		}
		return Size;
	}

	// "4096x4096", or "4096x4096 (loads at 1024x1024, Max Texture Size 1024)" when clamped.
	FString TextureSizeLabel(const UTexture2D* Tex)
	{
		const FIntPoint A = AuthoredTextureSize(Tex);
		const FIntPoint E = EffectiveTextureSize(Tex);
		if (E != A)
			return FString::Printf(TEXT("%dx%d (loads at %dx%d, Max Texture Size %d)"),
				A.X, A.Y, E.X, E.Y, Tex->MaxTextureSize);
		return FString::Printf(TEXT("%dx%d"), A.X, A.Y);
	}
}

FRPDAuditRule AuditRules::Rule_NonPowerOfTwo = {
	TEXT("NonPowerOfTwo"),
	TEXT("Texture dimensions are not power-of-two — may cause unexpected padding or mip issues."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			// Power-of-two is a property of the authored source, not the runtime size.
			const FIntPoint Sz = AuthoredTextureSize(Tex);
			if (!FMath::IsPowerOfTwo(Sz.X) || !FMath::IsPowerOfTwo(Sz.Y))
			{
				OutIssue.Detail = FString::Printf(TEXT("Dimensions: %dx%d (not power-of-two)."), Sz.X, Sz.Y);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_LargeTexture = {
	TEXT("LargeTexture"),
	TEXT("Texture is very large — consider if this resolution is needed."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			// Judge by what actually loads, so a capped source isn't falsely flagged.
			const FIntPoint Eff = EffectiveTextureSize(Tex);
			int32 MaxDimension = FMath::Max(Eff.X, Eff.Y);
			int32 WarnPx = RuleConfig.FindRef(TEXT("LargeTexture_Warning"), 4096);
			int32 CritPx = RuleConfig.FindRef(TEXT("LargeTexture_Critical"), 8192);

			if (MaxDimension > CritPx)
			{
				OutIssue.Detail = FString::Printf(TEXT("Dimensions: %s (max: %d)."), *TextureSizeLabel(Tex), MaxDimension);
				return true;
			}
			if (MaxDimension >= WarnPx)
			{
				OutIssue.Detail = FString::Printf(TEXT("Dimensions: %s — large resolution."), *TextureSizeLabel(Tex));
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_VirtualTextureMismatch = {
	TEXT("VirtualTextureMismatch"),
	TEXT("Virtual Texture streamer setting may be suboptimal for this texture's usage."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// Skip when project doesn't use virtual textures — all textures are expected non-VT.
		if (!AuditRules::bVTEnabled)
			return false;

		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			bool bIsVT = Tex->VirtualTextureStreaming;
			const FIntPoint Eff = EffectiveTextureSize(Tex);
			int64 TotalPixels = static_cast<int64>(Eff.X) * Eff.Y;
			// Large textures that aren't using VT
			if (TotalPixels >= 2048 * 2048 && !bIsVT)
			{
				OutIssue.Detail = FString::Printf(TEXT("%s — not using virtual texture streaming."), *TextureSizeLabel(Tex));
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_NeverStream = {
	TEXT("NeverStream"),
	TEXT("Texture has NeverStream enabled — always stays in VRAM. Consider disabling for large textures."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// Skip when texture streaming is disabled — NeverStream is the default and expected.
		if (!AuditRules::bTextureStreamingEnabled)
			return false;

		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			if (Tex->NeverStream)
			{
				// VRAM reflects the size actually resident (after Max Texture Size),
				// plus the full mip chain (~4/3 of the base level).
				const FIntPoint Eff = EffectiveTextureSize(Tex);
				int64 TotalPixels = static_cast<int64>(Eff.X) * Eff.Y;
				int64 EstimatedVRAM = (TotalPixels * 4 * 4) / 3;

				OutIssue.Detail = FString::Printf(
					TEXT("%s — NeverStream (~%.0f MB estimated VRAM). Disable streaming or reduce resolution if not needed in all levels."),
					*TextureSizeLabel(Tex), EstimatedVRAM / 1048576.0f);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_TextureLODGroup = {
	TEXT("TextureLODGroup"),
	TEXT("Texture LOD Group may be suboptimal for this texture type."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// LOD group hints are less meaningful when texture streaming is disabled.
		if (!AuditRules::bTextureStreamingEnabled)
			return false;

		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			const FIntPoint Eff = EffectiveTextureSize(Tex);
			int32 SX = Eff.X;
			int32 SY = Eff.Y;
			int64 TotalPixels = static_cast<int64>(SX) * SY;
			bool bIsUI = Tex->LODGroup == TextureGroup::TEXTUREGROUP_UI;
			bool bIsWorld = Tex->LODGroup == TextureGroup::TEXTUREGROUP_World;

			if (bIsUI && (SX > 1024 || SY > 1024))
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%s — UI texture with very high resolution. Consider downscaling or using TC_EditorIcon."),
					*TextureSizeLabel(Tex));
				return true;
			}

			if (bIsWorld && TotalPixels < 256 * 256)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%s — small texture in World group. Consider TEXTUREGROUP_UI or similar."),
					*TextureSizeLabel(Tex));
				OutIssue.SeverityScore = 1;
				return true;
			}
		}
		return false;
	}
};

// Rough bytes-per-pixel for common cooked formats. The editor format is usually
// PF_B8G8R8A8 (4 BPP), but the runtime cost depends on the compressed format:
//   DXT1/BC1: 0.5 BPP | DXT5/BC3: 1 BPP | BC4: 0.5 BPP | BC5: 1 BPP
//   BC6H: 1 BPP | BC7: 1 BPP | RGBA8: 4 BPP | G8: 1 BPP
// We default to a conservative 1 BPP (BC3/BC7 class) when guesting off the import
// type, since most game textures compress to one of those.
static double EstimatedCookedBPP(const UTexture2D* Tex)
{
	if (!Tex) return 1.0;

	// Greyscale/alpha-only → DXT1 or BC4
	if (Tex->CompressionSettings == TC_Grayscale ||
		Tex->CompressionSettings == TC_Alpha)
		return 0.5;

	// Normal maps → BC5
	if (Tex->CompressionSettings == TC_Normalmap)
		return 1.0;

	// HDR → BC6H (1 BPP)
	if (Tex->CompressionSettings == TC_HDR ||
		Tex->CompressionSettings == TC_HDR_Compressed)
		return 1.0;

	// Default textures (TC_Default, TC_Masks, TC_Displacementmap) → DXT1/BC1 or
	// BC3/BC7 depending on alpha. If it has alpha or is sRGB masked, assume 1 BPP.
	return 1.0;
}

// Estimate total VRAM for a fully-resident texture (all mips loaded).
// Formula: sum_{i=0}^{Mips-1} (max(1,W>>i) * max(1,H>>i)) * BPP
static int64 EstimateVRAM(const UTexture2D* Tex)
{
	const FIntPoint Eff = EffectiveTextureSize(Tex);
	const double BPP = EstimatedCookedBPP(Tex);
	int32 NumMips = Tex->GetNumMips();
	if (NumMips <= 0)
	{
		// If the texture hasn't been built yet, assume full chain down to 1x1.
		int32 MaxDim = FMath::Max(Eff.X, Eff.Y);
		NumMips = FMath::FloorLog2(MaxDim) + 1;
	}

	int64 TotalBytes = 0;
	int32 W = Eff.X;
	int32 H = Eff.Y;
	for (int32 Mip = 0; Mip < NumMips; ++Mip)
	{
		TotalBytes += static_cast<int64>(FMath::Max(1, W) * FMath::Max(1, H) * BPP);
		W >>= 1;
		H >>= 1;
	}
	return TotalBytes;
}

FRPDAuditRule AuditRules::Rule_TextureStreamingBudget = {
	TEXT("TextureStreamingBudget"),
	TEXT("Texture has a high estimated VRAM footprint — consider reducing resolution or improving compression."),
	UTexture2D::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UTexture2D* Tex = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			const FIntPoint Eff = EffectiveTextureSize(Tex);
			const int64 VRAM = EstimateVRAM(Tex);
			const int64 WarnBytes = static_cast<int64>(RuleConfig.FindRef(TEXT("TextureStreamingBudget_Warning"), 8)) * 1048576LL;
			const int64 CritBytes = static_cast<int64>(RuleConfig.FindRef(TEXT("TextureStreamingBudget_Critical"), 16)) * 1048576LL;

			if (VRAM > CritBytes)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%s — ~%.0f MB fully resident. High VRAM cost; consider reducing resolution or enabling streaming."),
					*TextureSizeLabel(Tex), static_cast<double>(VRAM) / 1048576.0);
				return true;
			}
			if (VRAM > WarnBytes)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%s — ~%.0f MB fully resident. Verify this texture needs the full resolution."),
					*TextureSizeLabel(Tex), static_cast<double>(VRAM) / 1048576.0);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};
