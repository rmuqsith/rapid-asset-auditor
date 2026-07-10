// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "RPDAuditResult.h"

/** Describes a single audit rule that can run against an asset */
struct FRPDAuditRule
{
	/** Unique rule identifier (e.g. "NoLODs") */
	FString RuleName;

	/** Human-readable description */
	FString Description;

	/** Which asset class this rule applies to */
	FName AssetClassName;

	/** Default severity if triggered */
	ERPDAuditSeverity DefaultSeverity = ERPDAuditSeverity::Warning;

	/** Score contribution (1-10) toward health metric */
	int32 SeverityScore = 5;

	/** The actual check function: return true if an issue was found */
	TFunction<bool(const FAssetData& AssetData, /* out */ FRPDAuditIssue& OutIssue)> CheckFunc;
};

/** Registry of all built-in audit rules */
struct FRPDAuditRuleRegistry
{
public:
	static FRPDAuditRuleRegistry& Get();

	/** Register a rule */
	void RegisterRule(const FRPDAuditRule& Rule);

	/** Get all rules for a given asset class */
	TArray<FRPDAuditRule> GetRulesForClass(FName ClassName) const;

	/** Get all rules */
	const TArray<FRPDAuditRule>& GetAllRules() const { return Rules; }

private:
	FRPDAuditRuleRegistry() = default;
	TArray<FRPDAuditRule> Rules;
};

// ── Rule Registration Macros ──

#define REGISTER_AUDIT_RULE(RuleVar) \
	struct FAutoRegister_##RuleVar \
	{ \
		FAutoRegister_##RuleVar() \
		{ \
			FRPDAuditRuleRegistry::Get().RegisterRule(RuleVar); \
		} \
	}; \
	static FAutoRegister_##RuleVar AutoRegister_##RuleVar;

// ── Built-in rule namespace ──

namespace AuditRules
{
	// Static Mesh
	extern FRPDAuditRule Rule_NoLODs;
	extern FRPDAuditRule Rule_NoLODsAndNaniteDisabled;
	extern FRPDAuditRule Rule_HighVertexCount;
	extern FRPDAuditRule Rule_HighMaterialCount;
	extern FRPDAuditRule Rule_HighUVChannelCount;
	extern FRPDAuditRule Rule_MissingCollision;
	extern FRPDAuditRule Rule_MissingStaticMeshMaterial;
	extern FRPDAuditRule Rule_SameMaterialMultipleSlots;

	// Skeletal Mesh
	extern FRPDAuditRule Rule_SkeletalMesh_NoLODs;
	extern FRPDAuditRule Rule_SkeletalMesh_HighVertexCount;
	extern FRPDAuditRule Rule_SkeletalMesh_HighMaterialCount;
	extern FRPDAuditRule Rule_MissingSkeletalMeshMaterial;

	// Texture
	extern FRPDAuditRule Rule_NonPowerOfTwo;
	extern FRPDAuditRule Rule_LargeTexture;
	extern FRPDAuditRule Rule_VirtualTextureMismatch;
	extern FRPDAuditRule Rule_NeverStream;
	extern FRPDAuditRule Rule_TextureLODGroup;

	// Sound
	extern FRPDAuditRule Rule_MissingSoundClass;
	extern FRPDAuditRule Rule_MissingAttenuation;
	extern FRPDAuditRule Rule_AudioCompression;
	extern FRPDAuditRule Rule_WAVLength;
	extern FRPDAuditRule Rule_NoConcurrencySettings;

	// Misc
	extern FRPDAuditRule Rule_MissingPhysicsAsset;
	extern FRPDAuditRule Rule_UnusedAsset;
	extern FRPDAuditRule Rule_ProjectRedirectors;

	// Animation
	extern FRPDAuditRule Rule_LongAnimSequence;

	// Material / Material Instance
	extern FRPDAuditRule Rule_MasterMaterialBloat;
	extern FRPDAuditRule Rule_MaterialInstanceBloat;
	extern FRPDAuditRule Rule_MaterialInstanceMinimalOverrides;
	extern FRPDAuditRule Rule_ListAllMasterMaterials;
	extern FRPDAuditRule Rule_BlendModeMismatch;
	extern FRPDAuditRule Rule_ShadingModelMismatch;
	extern FRPDAuditRule Rule_HighTextureSampling;
	extern FRPDAuditRule Rule_OneOffMaterial;
	extern FRPDAuditRule Rule_DuplicateMaterialInstance;
	extern FRPDAuditRule Rule_DirectMasterMaterialOnStaticMesh;
	extern FRPDAuditRule Rule_DirectMasterMaterialOnSkeletalMesh;
	extern FRPDAuditRule Rule_UnusedMasterParameters;

	// Blueprint
	extern FRPDAuditRule Rule_BlueprintComplexParents;
	extern FRPDAuditRule Rule_BlueprintComplexity;
	extern FRPDAuditRule Rule_BlueprintExposedParams;
	extern FRPDAuditRule Rule_WidgetBlueprintComplexity;


	/** If true, Nanite rules are inverted: warn when Nanite IS enabled instead of when disabled */
	extern bool bNaniteInverted;

	/** Rule configuration overrides: ThresholdName -> Value */
	extern TMap<FString, int32> RuleConfig;

	/** Naming conventions: ClassName -> Prefix */
	extern TMap<FString, FString> NamingPrefixes;

	/** Rule enable/disable: RuleName -> enabled */
	extern TMap<FString, bool> RuleEnabled;

	/** List of ignored asset object paths */
	extern TArray<FString> IgnoredAssetPaths;

	/** Ignore an asset by its object path */
	void IgnoreAsset(const FString& AssetPath);

	/** Remove an asset from ignore list */
	void UnignoreAsset(const FString& AssetPath);

	/** Check if an asset is ignored */
	bool IsAssetIgnored(const FString& AssetPath);

	/** Initializes all built-in rules */
	void RegisterBuiltInRules();

	/** Reset all rule configs to defaults */
	void ResetRuleConfig();

	/** Apply a platform preset ("Mobile / VR", "PC", "Console") to thresholds + Nanite mode. */
	void ApplyPreset(const FString& PresetName);

	/** Load config from GConfig (Config/RPDAssetAuditor.ini) */
	void LoadConfig();

	/** Save config to GConfig */
	void SaveConfig();

	/** Get config file path for display */
	FString GetConfigPath();

	/** Config profiles */
	extern FString CurrentProfile;
	extern TArray<TSharedPtr<FString>> ProfileList;

	/** Save current config as a named profile */
	void SaveProfile(const FString& ProfileName);

	/** Load a named profile */
	void LoadProfile(const FString& ProfileName);

	/** Delete a named profile */
	void DeleteProfile(const FString& ProfileName);

	/** Scan available profiles from config file */
	void ScanProfiles();
};
