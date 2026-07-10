// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "MaterialDomain.h" // EMaterialDomain / MD_Surface
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

/**
 * Check if a static switch parameter is actually overridden (value differs from parent).
 */
static bool IsStaticSwitchOverridden(UMaterialInstanceConstant* MIC, const FMaterialParameterInfo& ParamInfo)
{
	if (!MIC || !MIC->Parent)
		return false;

	bool bMICValue = false;
	FGuid Guid;
	if (!MIC->GetStaticSwitchParameterValue(ParamInfo.Name, bMICValue, Guid))
		return false;

	bool bParentValue = false;
	if (!MIC->Parent->GetStaticSwitchParameterValue(ParamInfo.Name, bParentValue, Guid))
		return false;

	return bMICValue != bParentValue;
}

/**
 * Count how many static switch parameters are overridden on this MIC.
 */
/**
 * BlendMode enum integer → human-readable string.
 */
static FString BlendModeToString(int32 Mode)
{
	switch (Mode)
	{
	case 0:  return TEXT("Opaque");
	case 1:  return TEXT("Masked");
	case 2:  return TEXT("Translucent");
	case 3:  return TEXT("Additive");
	case 4:  return TEXT("Modulate");
	case 5:  return TEXT("AlphaComposite");
	case 6:  return TEXT("AlphaHoldout");
	default: return FString::Printf(TEXT("Unknown (%d)"), Mode);
	}
}

static int32 CountOverriddenStaticSwitches(UMaterialInstanceConstant* MIC)
{
	if (!MIC)
		return 0;

	TArray<FMaterialParameterInfo> SwitchInfos;
	TArray<FGuid> SwitchIds;
	MIC->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchIds);

	int32 Count = 0;
	for (const FMaterialParameterInfo& Info : SwitchInfos)
	{
		if (IsStaticSwitchOverridden(MIC, Info))
		{
			Count++;
		}
	}
	return Count;
}

// True if any other asset references this asset's package (i.e. it is used).
static bool HasAnyReferencer(const FAssetData& AssetData)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetIdentifier> Referencers;
	Registry.GetReferencers(AssetData.PackageName, Referencers);
	return Referencers.Num() > 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Rule 1 — Master Material Bloat
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_MasterMaterialBloat = {
	TEXT("MasterMaterialBloat"),
	TEXT("Master material has excessive parameters — bloated shader from marketplace packs."),
	UMaterial::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 7,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterial* Mat = Cast<UMaterial>(AssetData.GetAsset()))
		{
			TArray<FMaterialParameterInfo> ParamInfo;
			TArray<FGuid> ParamIds;

			Mat->GetAllScalarParameterInfo(ParamInfo, ParamIds);
			int32 ScalarCount = ParamInfo.Num();

			ParamInfo.Empty(); ParamIds.Empty();
			Mat->GetAllTextureParameterInfo(ParamInfo, ParamIds);
			int32 TextureCount = ParamInfo.Num();

			ParamInfo.Empty(); ParamIds.Empty();
			Mat->GetAllStaticSwitchParameterInfo(ParamInfo, ParamIds);
			int32 SwitchCount = ParamInfo.Num();

			int32 TotalParams = ScalarCount + TextureCount + SwitchCount;

			if (ScalarCount > 20)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d scalar parameters — extreme bloat. Consider splitting into specialized masters."),
					ScalarCount);
				return true;
			}
			if (ScalarCount > 15)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d scalar parameters — excessive. Consider reducing unneeded parameters or splitting into simpler masters."),
					ScalarCount);
				OutIssue.SeverityScore = 6;
				return true;
			}
			if (TextureCount > 12)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d texture parameters — heavy GPU texture sampling cost."),
					TextureCount);
				return true;
			}
			if (TextureCount > 8)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d texture parameters — consider reducing texture count for performance."),
					TextureCount);
				OutIssue.SeverityScore = 5;
				return true;
			}
			if (SwitchCount > 12)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d static switches — very complex material, high shader permutation count."),
					SwitchCount);
				return true;
			}
			if (TotalParams > 30)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d total parameters — extremely bloated master material."),
					TotalParams);
				return true;
			}
			if (TotalParams > 20)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d total parameters — above healthy threshold for a master material."),
					TotalParams);
				OutIssue.SeverityScore = 4;
				return true;
			}
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Rule 2 — Material Instance Bloat (Zero Overrides)
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_MaterialInstanceBloat = {
	TEXT("MaterialInstanceBloat"),
	// NOTE: a *used* zero-override instance is the recommended instance-on-mesh pattern
	// (keeps masters off meshes, leaves a hook for future tweaks) — it is NOT flagged.
	// Only an *unused* zero-override instance is deletable cruft.
	TEXT("Unused material instance that overrides nothing — deletable. Used zero-override instances are a valid convention and are not flagged."),
	UMaterialInstanceConstant::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			int32 TotalOverrides = MIC->ScalarParameterValues.Num()
				+ MIC->TextureParameterValues.Num()
				+ MIC->VectorParameterValues.Num();
			TotalOverrides += CountOverriddenStaticSwitches(MIC);

			// Applying an override-free instance to a mesh is correct practice, so a
			// zero-override instance is only a problem when nothing uses it.
			if (TotalOverrides != 0 || HasAnyReferencer(AssetData))
				return false;

			const FString Parent = MIC->Parent ? MIC->Parent->GetName() : TEXT("(none)");
			OutIssue.ParentName = Parent;
			OutIssue.Detail = FString::Printf(
				TEXT("Unused instance with no overrides (parent '%s') — nothing references it; safe to delete."),
				*Parent);
			return true;
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Rule 3 — Material Instance Minimal Overrides
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_MaterialInstanceMinimalOverrides = {
	TEXT("MaterialInstanceMinimalOverrides"),
	TEXT("Material instance overrides very few parameters on a complex master — could be simplified."),
	UMaterialInstanceConstant::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			int32 TotalOverrides = MIC->ScalarParameterValues.Num()
				+ MIC->TextureParameterValues.Num()
				+ MIC->VectorParameterValues.Num();

			TotalOverrides += CountOverriddenStaticSwitches(MIC);

			// Subtract irrelevant default overrides (e.g. RefractionDepthBias)
			for (const auto& SPV : MIC->ScalarParameterValues)
			{
				if (SPV.ParameterInfo.Name == TEXT("RefractionDepthBias"))
				{
					TotalOverrides--;
				}
			}

			// Only warn if has overrides (but not 0 - referencing MI is best practice)
			// and total is very low on a complex parent
			if (TotalOverrides > 0 && TotalOverrides <= 2 && MIC->Parent)
			{
				TArray<FMaterialParameterInfo> ParamInfo;
				TArray<FGuid> ParamIds;
				MIC->Parent->GetAllScalarParameterInfo(ParamInfo, ParamIds);
				int32 ParentScalars = ParamInfo.Num();

				ParamInfo.Empty(); ParamIds.Empty();
				MIC->Parent->GetAllTextureParameterInfo(ParamInfo, ParamIds);
				int32 ParentTextures = ParamInfo.Num();

				int32 ParentTotal = ParentScalars + ParentTextures;

				if (ParentTotal > 10)
				{
					FString OverrideList;
					auto AppendName = [&](const FName& ParamName)
					{
						if (!OverrideList.IsEmpty()) OverrideList += TEXT(", ");
						OverrideList += ParamName.ToString();
					};

					for (const auto& SPV : MIC->ScalarParameterValues)
						AppendName(SPV.ParameterInfo.Name);
					for (const auto& TPV : MIC->TextureParameterValues)
						AppendName(TPV.ParameterInfo.Name);
					for (const auto& VPV : MIC->VectorParameterValues)
						AppendName(VPV.ParameterInfo.Name);

					{
						TArray<FMaterialParameterInfo> SwitchInfos;
						TArray<FGuid> SwitchIds;
						MIC->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchIds);
						for (const FMaterialParameterInfo& Info : SwitchInfos)
						{
							if (IsStaticSwitchOverridden(MIC, Info))
							{
								AppendName(Info.Name);
							}
						}
					}

					FString Parent = MIC->Parent->GetName();
					OutIssue.ParentName = Parent;
					OutIssue.Detail = FString::Printf(
						TEXT("%d override(s) on complex master '%s' (%d params). Overrides: [%s]. Consider if a simpler parent is needed."),
						TotalOverrides, *Parent, ParentTotal, *OverrideList);
					return true;
				}
			}
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Rule 4 — Blend Mode Mismatch
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_BlendModeMismatch = {
	TEXT("BlendModeMismatch"),
	TEXT("Material instance overrides blend mode from parent — verify this is intentional."),
	UMaterialInstanceConstant::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			OutIssue.ParentName = MIC->Parent ? MIC->Parent->GetName() : TEXT("(none)");

			if (MIC->Parent && MIC->BasePropertyOverrides.bOverride_BlendMode)
			{
				EBlendMode InstanceBlend = MIC->BasePropertyOverrides.BlendMode;
				EBlendMode ParentBlend = MIC->Parent->GetBlendMode();

				if (InstanceBlend != ParentBlend)
				{
					OutIssue.Detail = FString::Printf(
						TEXT("Parent is '%s', instance overrides to '%s'. This may be unintentional — verify the override is needed."),
						*BlendModeToString((int32)ParentBlend), *BlendModeToString((int32)InstanceBlend));
					return true;
				}
			}
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Rule 6 — Shading Model Mismatch
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_ShadingModelMismatch = {
	TEXT("ShadingModelMismatch"),
	TEXT("Material instance uses a different shading model from parent — verify lighting intent."),
	UMaterialInstanceConstant::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			OutIssue.ParentName = MIC->Parent ? MIC->Parent->GetName() : TEXT("(none)");

			if (MIC->BasePropertyOverrides.bOverride_ShadingModel)
			{
				int32 ShadingModelInt = static_cast<int32>(MIC->BasePropertyOverrides.ShadingModel);
				OutIssue.Detail = FString::Printf(
					TEXT("Instance overrides ShadingModel to %d. Verify this is intentional — mismatched shading models can break lighting."),
					ShadingModelInt);
				return true;
			}
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Rule 7 — High Texture Sampling
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_HighTextureSampling = {
	TEXT("HighTextureSampling"),
	TEXT("Material samples many unique textures — check if all are necessary."),
	NAME_None,
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInterface* MatInterface = Cast<UMaterialInterface>(AssetData.GetAsset()))
		{
			// Set parent name for Material Instances so column is always filled
			if (UMaterialInstanceConstant* TexMIC = Cast<UMaterialInstanceConstant>(MatInterface))
			{
				OutIssue.ParentName = TexMIC->Parent ? TexMIC->Parent->GetName() : TEXT("(none)");
			}

			TArray<FMaterialParameterInfo> ParamInfo;
			TArray<FGuid> ParamIds;
			MatInterface->GetAllTextureParameterInfo(ParamInfo, ParamIds);
			int32 TextureCount = ParamInfo.Num();

			int32 WarnThreshold = RuleConfig.FindRef(TEXT("HighTextureSampling_Warning"), 6);
			int32 CritThreshold = RuleConfig.FindRef(TEXT("HighTextureSampling_Critical"), 9);

			if (TextureCount > CritThreshold)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d texture parameters — high GPU cost. Each unique texture uses sampler bandwidth."),
					TextureCount);
				return true;
			}
			if (TextureCount > WarnThreshold)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d texture parameters — consider if all are needed for performance."),
					TextureCount);
				OutIssue.SeverityScore = 3;
				return true;
			}
		}
		return false;
	}
};

// Rule 8 — List All Master Materials
// ═══════════════════════════════════════════════════════════════════════════
FRPDAuditRule AuditRules::Rule_ListAllMasterMaterials = {
	TEXT("ListAllMasterMaterials"),
	TEXT("Master materials with very few instances — consolidation candidates. Heavily-reused masters are left alone."),
	// Must be the concrete class: the dispatcher matches AssetData's concrete class
	// name (e.g. "Material"), never the abstract "MaterialInterface" base.
	UMaterial::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 1,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterial* Mat = Cast<UMaterial>(AssetData.GetAsset()))
		{
			// OutIssue.ChildCount (seeded by the manager) = number of material instances
			// deriving from this master. A well-reused master is working as intended, so
			// only surface low-reuse ones as consolidation candidates. Skip 0 (masters
			// applied directly to meshes / caught separately by UnusedAsset).
			const int32 Instances = OutIssue.ChildCount;
			const int32 LowReuseMax = RuleConfig.FindRef(TEXT("MasterMaterial_LowReuseMax"), 2);
			if (Instances < 1 || Instances > LowReuseMax)
				return false;

			int32 TexSampleCount = 0;
			for (UMaterialExpression* Expr : Mat->GetExpressions())
			{
				if (Expr && Expr->GetClass()->GetFName().ToString().Contains(TEXT("TextureSample")))
					TexSampleCount++;
			}
			OutIssue.Detail = FString::Printf(
				TEXT("Master material with only %d instance(s) and %d texture sample(s) — consolidation candidate (merge into a shared master, or use instances of an existing one)."),
				Instances, TexSampleCount);
			return true;
		}
		return false;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// Material sprawl / consolidation checks
// ═══════════════════════════════════════════════════════════════════════════

// Count static + skeletal meshes that reference this asset's package.
static int32 CountMeshReferencers(const FAssetData& AssetData)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetIdentifier> Referencers;
	Registry.GetReferencers(AssetData.PackageName, Referencers);

	int32 Count = 0;
	for (const FAssetIdentifier& Ref : Referencers)
	{
		TArray<FAssetData> RefAssets;
		Registry.GetAssetsByPackageName(Ref.PackageName, RefAssets);
		for (const FAssetData& RA : RefAssets)
		{
			UClass* C = RA.GetClass();
			if (C == UStaticMesh::StaticClass() || C == USkeletalMesh::StaticClass())
				Count++;
		}
	}
	return Count;
}

// Direct instances (parent == Master) of a master material. Assets are already
// loaded during the scan, so GetAsset() here is a cheap cache hit.
static TArray<UMaterialInstanceConstant*> LoadDirectInstancesOf(UMaterialInterface* Master)
{
	TArray<UMaterialInstanceConstant*> Out;
	if (!Master)
		return Out;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetIdentifier> Referencers;
	Registry.GetReferencers(Master->GetPackage()->GetFName(), Referencers);

	for (const FAssetIdentifier& Ref : Referencers)
	{
		TArray<FAssetData> RefAssets;
		Registry.GetAssetsByPackageName(Ref.PackageName, RefAssets);
		for (const FAssetData& RA : RefAssets)
		{
			if (RA.GetClass() == UMaterialInstanceConstant::StaticClass())
			{
				if (UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(RA.GetAsset()))
				{
					if (MI->Parent == Master)
						Out.Add(MI);
				}
			}
		}
	}
	return Out;
}

// A stable signature of an instance's effective parameters, used to spot twins.
static FString MIOverrideSignature(UMaterialInstanceConstant* MIC)
{
	TArray<FString> Parts;
	for (const FScalarParameterValue& P : MIC->ScalarParameterValues)
		Parts.Add(FString::Printf(TEXT("S:%s=%.4f"), *P.ParameterInfo.Name.ToString(), P.ParameterValue));
	for (const FVectorParameterValue& P : MIC->VectorParameterValues)
		Parts.Add(FString::Printf(TEXT("V:%s=%s"), *P.ParameterInfo.Name.ToString(), *P.ParameterValue.ToString()));
	for (const FTextureParameterValue& P : MIC->TextureParameterValues)
		Parts.Add(FString::Printf(TEXT("T:%s=%s"), *P.ParameterInfo.Name.ToString(),
			P.ParameterValue ? *P.ParameterValue->GetPathName() : TEXT("null")));

	TArray<FMaterialParameterInfo> Switches; TArray<FGuid> SwitchIds;
	MIC->GetAllStaticSwitchParameterInfo(Switches, SwitchIds);
	for (const FMaterialParameterInfo& Info : Switches)
	{
		bool bVal = false; FGuid G;
		if (MIC->GetStaticSwitchParameterValue(Info.Name, bVal, G))
			Parts.Add(FString::Printf(TEXT("W:%s=%d"), *Info.Name.ToString(), bVal ? 1 : 0));
	}
	Parts.Sort();
	return FString::Join(Parts, TEXT("|"));
}

// ── One-off master material: a master with no instances, used by a single mesh ──
FRPDAuditRule AuditRules::Rule_OneOffMaterial = {
	TEXT("OneOffMaterial"),
	TEXT("Master material has no instances and is used by a single mesh — a one-off that fuels material sprawl."),
	UMaterial::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 4,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterial* Mat = Cast<UMaterial>(AssetData.GetAsset()))
		{
			// OutIssue.ChildCount (instances) is seeded by the manager. Only masters
			// with no instances qualify — low-instance masters are the sibling rule's job.
			if (OutIssue.ChildCount == 0 && CountMeshReferencers(AssetData) == 1)
			{
				OutIssue.Detail = TEXT("Used by a single mesh with no instances — a one-off master. Fold into a shared master, or make it an instance so it can be reused/retuned centrally.");
				return true;
			}
		}
		return false;
	}
};

// ── Duplicate material instance: same parent + identical overrides as a sibling ──
FRPDAuditRule AuditRules::Rule_DuplicateMaterialInstance = {
	TEXT("DuplicateMaterialInstance"),
	TEXT("Material instance has identical overrides to a sibling of the same parent — redundant, safe to merge."),
	UMaterialInstanceConstant::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			if (!MIC->Parent)
				return false;

			TArray<UMaterialInstanceConstant*> Siblings = LoadDirectInstancesOf(MIC->Parent);
			if (Siblings.Num() < 2 || Siblings.Num() > 200) // skip trivial / pathological groups
				return false;

			const FString MySig = MIOverrideSignature(MIC);
			const FString MyName = MIC->GetName();
			for (UMaterialInstanceConstant* Sib : Siblings)
			{
				if (Sib == MIC) continue;
				if (MIOverrideSignature(Sib) == MySig)
				{
					// Flag only the non-canonical copies (name-ordered) so the group
					// keeps one and the rest are marked redundant — avoids flagging all.
					if (MyName > Sib->GetName())
					{
						OutIssue.ParentName = MIC->Parent->GetName();
						OutIssue.Detail = FString::Printf(
							TEXT("Identical overrides to '%s' (same parent '%s') — redundant. Repoint users to one and delete the duplicate."),
							*Sib->GetName(), *MIC->Parent->GetName());
						return true;
					}
				}
			}
		}
		return false;
	}
};

// ── Direct master material on a static mesh slot (should be an instance) ──
FRPDAuditRule AuditRules::Rule_DirectMasterMaterialOnStaticMesh = {
	TEXT("DirectMasterMaterialOnStaticMesh"),
	TEXT("Static mesh slot uses a master material directly instead of an instance."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 4,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			UMaterialInterface* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
			TArray<FString> Direct;
			for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
			{
				UMaterialInterface* MI = Slot.MaterialInterface;
				// A master is a UMaterial; an instance is not. Skip null/default (that's a
				// "missing material", covered elsewhere).
				if (MI && MI != DefaultMat && Cast<UMaterial>(MI))
					Direct.AddUnique(MI->GetName());
			}
			if (Direct.Num() > 0)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d slot(s) use a master directly (%s) instead of an instance — can't retune per-use and painful to consolidate later."),
					Direct.Num(), *FString::Join(Direct, TEXT(", ")));
				return true;
			}
		}
		return false;
	}
};

// ── Direct master material on a skeletal mesh slot ──
FRPDAuditRule AuditRules::Rule_DirectMasterMaterialOnSkeletalMesh = {
	TEXT("DirectMasterMaterialOnSkeletalMesh"),
	TEXT("Skeletal mesh slot uses a master material directly instead of an instance."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 4,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			UMaterialInterface* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
			TArray<FString> Direct;
			for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
			{
				UMaterialInterface* MI = Slot.MaterialInterface;
				if (MI && MI != DefaultMat && Cast<UMaterial>(MI))
					Direct.AddUnique(MI->GetName());
			}
			if (Direct.Num() > 0)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d slot(s) use a master directly (%s) instead of an instance — can't retune per-use and painful to consolidate later."),
					Direct.Num(), *FString::Join(Direct, TEXT(", ")));
				return true;
			}
		}
		return false;
	}
};

// ── Unused master parameters: params no instance ever overrides = dead bloat ──
FRPDAuditRule AuditRules::Rule_UnusedMasterParameters = {
	TEXT("UnusedMasterParameters"),
	TEXT("Master exposes parameters that no instance ever overrides — dead bloat that can be trimmed."),
	UMaterial::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UMaterial* Mat = Cast<UMaterial>(AssetData.GetAsset()))
		{
			// Need instances to know which params are actually used. ChildCount seeded by manager.
			if (OutIssue.ChildCount < 2)
				return false;

			// All exposed parameter names on the master.
			TSet<FName> AllParams;
			TArray<FMaterialParameterInfo> Info; TArray<FGuid> Ids;
			auto AddNames = [&]() { for (const FMaterialParameterInfo& I : Info) AllParams.Add(I.Name); Info.Empty(); Ids.Empty(); };
			Mat->GetAllScalarParameterInfo(Info, Ids); AddNames();
			Mat->GetAllVectorParameterInfo(Info, Ids); AddNames();
			Mat->GetAllTextureParameterInfo(Info, Ids); AddNames();
			Mat->GetAllStaticSwitchParameterInfo(Info, Ids); AddNames();
			if (AllParams.Num() == 0)
				return false;

			// Union of parameter names any instance actually overrides.
			TSet<FName> Overridden;
			for (UMaterialInstanceConstant* MI : LoadDirectInstancesOf(Mat))
			{
				for (const FScalarParameterValue& P : MI->ScalarParameterValues) Overridden.Add(P.ParameterInfo.Name);
				for (const FVectorParameterValue& P : MI->VectorParameterValues) Overridden.Add(P.ParameterInfo.Name);
				for (const FTextureParameterValue& P : MI->TextureParameterValues) Overridden.Add(P.ParameterInfo.Name);
				TArray<FMaterialParameterInfo> Sw; TArray<FGuid> SwIds;
				MI->GetAllStaticSwitchParameterInfo(Sw, SwIds);
				for (const FMaterialParameterInfo& S : Sw)
					if (IsStaticSwitchOverridden(MI, S)) Overridden.Add(S.Name);
			}

			int32 Unused = 0;
			for (const FName& P : AllParams)
				if (!Overridden.Contains(P)) Unused++;

			const int32 MinUnused = AuditRules::RuleConfig.FindRef(TEXT("MasterMaterial_UnusedParamsMin"), 5);
			if (Unused >= MinUnused)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d of %d exposed parameters are never overridden by any of its %d instance(s) — dead bloat; trim from the master."),
					Unused, AllParams.Num(), OutIssue.ChildCount);
				return true;
			}
		}
		return false;
	}
};
