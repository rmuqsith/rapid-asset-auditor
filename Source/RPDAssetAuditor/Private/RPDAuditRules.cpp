// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

FRPDAuditRuleRegistry& FRPDAuditRuleRegistry::Get()
{
	static FRPDAuditRuleRegistry Instance;
	return Instance;
}

void FRPDAuditRuleRegistry::RegisterRule(const FRPDAuditRule& Rule)
{
	for (const FRPDAuditRule& Existing : Rules)
	{
		if (Existing.RuleName.Equals(Rule.RuleName))
			return;
	}
	Rules.Add(Rule);
}

TArray<FRPDAuditRule> FRPDAuditRuleRegistry::GetRulesForClass(FName ClassName) const
{
	TArray<FRPDAuditRule> Result;
	for (const FRPDAuditRule& Rule : Rules)
	{
		if (Rule.AssetClassName == ClassName)
		{
			Result.Add(Rule);
		}
	}
	return Result;
}

// Default to a NON-Nanite project (bNaniteInverted == true ⇒ "Project uses Nanite"
// unchecked). Nanite rules then flag meshes that have Nanite enabled.
bool AuditRules::bNaniteInverted = true;
TMap<FString, int32> AuditRules::RuleConfig;
TMap<FString, FString> AuditRules::NamingPrefixes;
TMap<FString, bool> AuditRules::RuleEnabled;
TArray<FString> AuditRules::IgnoredAssetPaths;

void AuditRules::IgnoreAsset(const FString& AssetPath)
{
	IgnoredAssetPaths.AddUnique(AssetPath);
	SaveConfig();
}

void AuditRules::UnignoreAsset(const FString& AssetPath)
{
	IgnoredAssetPaths.Remove(AssetPath);
	SaveConfig();
}

bool AuditRules::IsAssetIgnored(const FString& AssetPath)
{
	return IgnoredAssetPaths.Contains(AssetPath);
}

void AuditRules::ApplyPreset(const FString& PresetName)
{
	// Start from defaults, then override the platform-sensitive thresholds. Rule
	// toggles and naming prefixes are left as-is so a preset only retunes budgets.
	ResetRuleConfig();

	auto Set = [](const TCHAR* Key, int32 Val) { RuleConfig.Add(Key, Val); };

	if (PresetName == TEXT("Mobile / VR"))
	{
		// Tight budgets; Nanite is unavailable on mobile/Quest, so treat as non-Nanite.
		bNaniteInverted = true;
		Set(TEXT("HighVertexCount_Warning"), 1000);
		Set(TEXT("HighVertexCount_Critical"), 3000);
		Set(TEXT("SkeletalVertex_Warning"), 3000);
		Set(TEXT("SkeletalVertex_Critical"), 8000);
		Set(TEXT("LargeTexture_Warning"), 1024);
		Set(TEXT("LargeTexture_Critical"), 2048);
		Set(TEXT("NoLODs_MinVerts"), 500);
		Set(TEXT("HighMaterialCount_Warning"), 2);
		Set(TEXT("HighMaterialCount_Critical"), 4);
	}
	else if (PresetName == TEXT("Console"))
	{
		bNaniteInverted = false;
		Set(TEXT("HighVertexCount_Warning"), 4000);
		Set(TEXT("HighVertexCount_Critical"), 12000);
		Set(TEXT("SkeletalVertex_Warning"), 12000);
		Set(TEXT("SkeletalVertex_Critical"), 30000);
		Set(TEXT("LargeTexture_Warning"), 2048);
		Set(TEXT("LargeTexture_Critical"), 4096);
		Set(TEXT("NoLODs_MinVerts"), 1500);
		Set(TEXT("HighMaterialCount_Warning"), 3);
		Set(TEXT("HighMaterialCount_Critical"), 6);
	}
	else // "PC" (default / most permissive)
	{
		bNaniteInverted = false;
		Set(TEXT("HighVertexCount_Warning"), 5000);
		Set(TEXT("HighVertexCount_Critical"), 15000);
		Set(TEXT("SkeletalVertex_Warning"), 15000);
		Set(TEXT("SkeletalVertex_Critical"), 40000);
		Set(TEXT("LargeTexture_Warning"), 4096);
		Set(TEXT("LargeTexture_Critical"), 8192);
		Set(TEXT("NoLODs_MinVerts"), 2000);
		Set(TEXT("HighMaterialCount_Warning"), 4);
		Set(TEXT("HighMaterialCount_Critical"), 8);
	}
}

void AuditRules::ResetRuleConfig()
{
	// Non-Nanite project by default (see bNaniteInverted definition above).
	bNaniteInverted = true;

	RuleConfig.Empty();
	RuleConfig.Add(TEXT("HighTextureSampling_Warning"), 6);
	RuleConfig.Add(TEXT("HighTextureSampling_Critical"), 9);
	RuleConfig.Add(TEXT("LargeTexture_Warning"), 4096);
	RuleConfig.Add(TEXT("LargeTexture_Critical"), 8192);
	RuleConfig.Add(TEXT("HighVertexCount_Warning"), 5000);
	RuleConfig.Add(TEXT("HighVertexCount_Critical"), 10000);
	RuleConfig.Add(TEXT("NoLODs_MinVerts"), 1000);
	RuleConfig.Add(TEXT("HighMaterialCount_Warning"), 5);
	RuleConfig.Add(TEXT("HighMaterialCount_Critical"), 8);
	RuleConfig.Add(TEXT("MasterMaterial_LowReuseMax"), 2);
	RuleConfig.Add(TEXT("MasterMaterial_UnusedParamsMin"), 5);
	RuleConfig.Add(TEXT("SkeletalVertex_Warning"), 10000);
	RuleConfig.Add(TEXT("SkeletalVertex_Critical"), 20000);
	RuleConfig.Add(TEXT("SkeletalMatCount_Warning"), 5);
	RuleConfig.Add(TEXT("SkeletalMatCount_Critical"), 8);
	RuleConfig.Add(TEXT("LongAnimSequence_Warning"), 30);
	RuleConfig.Add(TEXT("HighUVChannel_Warning"), 3);
	RuleConfig.Add(TEXT("WAVLength_Warning"), 10);
	RuleConfig.Add(TEXT("WAVLength_Critical"), 30);

	// Naming convention defaults
	NamingPrefixes.Empty();
	NamingPrefixes.Add(TEXT("SM_"), TEXT("SM_"));
	NamingPrefixes.Add(TEXT("SK_"), TEXT("SK_"));
	NamingPrefixes.Add(TEXT("T_"), TEXT("T_"));
	NamingPrefixes.Add(TEXT("S_"), TEXT("S_"));
	NamingPrefixes.Add(TEXT("M_"), TEXT("M_"));
	NamingPrefixes.Add(TEXT("PPM_"), TEXT("PPM_"));
	NamingPrefixes.Add(TEXT("MI_"), TEXT("MI_"));
	NamingPrefixes.Add(TEXT("AS_"), TEXT("AS_"));

	// Default all rules enabled
	RuleEnabled.Empty();
	RuleEnabled.Add(TEXT("NoLODs"), true);
	RuleEnabled.Add(TEXT("NoLODsAndNaniteDisabled"), true);
	RuleEnabled.Add(TEXT("HighVertexCount"), true);
	RuleEnabled.Add(TEXT("HighMaterialCount"), true);
	RuleEnabled.Add(TEXT("MissingCollision"), true);
	RuleEnabled.Add(TEXT("SameMaterialMultipleSlots"), true);
	RuleEnabled.Add(TEXT("SkeletalMesh_NoLODs"), true);
	RuleEnabled.Add(TEXT("SkeletalMesh_HighVertexCount"), true);
	RuleEnabled.Add(TEXT("SkeletalMesh_HighMaterialCount"), true);
	RuleEnabled.Add(TEXT("NonPowerOfTwo"), true);
	RuleEnabled.Add(TEXT("LargeTexture"), true);
	RuleEnabled.Add(TEXT("VirtualTextureMismatch"), true);
	RuleEnabled.Add(TEXT("NeverStream"), true);
	RuleEnabled.Add(TEXT("TextureLODGroup"), true);
	RuleEnabled.Add(TEXT("MissingSoundClass"), true);
	RuleEnabled.Add(TEXT("MissingAttenuation"), true);
	RuleEnabled.Add(TEXT("AudioCompression"), true);
	RuleEnabled.Add(TEXT("BlueprintComplexParents"), false); // heuristic unreliable — opt-in only
	RuleEnabled.Add(TEXT("BlueprintComplexity"), true);
	RuleEnabled.Add(TEXT("BlueprintExposedParams"), true);
	RuleEnabled.Add(TEXT("WidgetBlueprintComplexity"), true);
	RuleEnabled.Add(TEXT("MissingPhysicsAsset"), true);
	RuleEnabled.Add(TEXT("UnusedAsset"), true);
	RuleEnabled.Add(TEXT("LongAnimSequence"), true);
	RuleEnabled.Add(TEXT("MasterMaterialBloat"), true);
	RuleEnabled.Add(TEXT("MaterialInstanceBloat"), true);
	RuleEnabled.Add(TEXT("MaterialInstanceMinimalOverrides"), true);
	RuleEnabled.Add(TEXT("BlendModeMismatch"), true);
	RuleEnabled.Add(TEXT("ShadingModelMismatch"), true);
	RuleEnabled.Add(TEXT("HighTextureSampling"), true);
	RuleEnabled.Add(TEXT("HighUVChannelCount"), true);
	RuleEnabled.Add(TEXT("MissingStaticMeshMaterial"), true);
	RuleEnabled.Add(TEXT("MissingSkeletalMeshMaterial"), true);
	RuleEnabled.Add(TEXT("WAVLength"), true);
	RuleEnabled.Add(TEXT("NoConcurrencySettings"), true);
	RuleEnabled.Add(TEXT("ListAllMasterMaterials"), true);
	RuleEnabled.Add(TEXT("OneOffMaterial"), true);
	RuleEnabled.Add(TEXT("DuplicateMaterialInstance"), true);
	RuleEnabled.Add(TEXT("DirectMasterMaterialOnStaticMesh"), true);
	RuleEnabled.Add(TEXT("DirectMasterMaterialOnSkeletalMesh"), true);
	RuleEnabled.Add(TEXT("UnusedMasterParameters"), true);
	RuleEnabled.Add(TEXT("ProjectRedirectors"), true);
}

FString AuditRules::GetConfigPath()
{
	return FPaths::ProjectConfigDir() / TEXT("RPDAssetAuditor.ini");
}

FString AuditRules::CurrentProfile;
TArray<TSharedPtr<FString>> AuditRules::ProfileList;

// Profile names are tracked in an explicit "ProfileNames" array. It lives in its
// OWN section (not "RPDAssetAuditor") because SaveConfig() empties that section on
// every save, which would otherwise wipe the profile list. (The previous
// GetPerObjectConfigSections approach searched by section *suffix* while profiles
// were written with a "Profile " *prefix*, so it never rediscovered them.)
static const TCHAR* GProfileNamesSection = TEXT("RPDAuditProfiles");
static const TCHAR* GProfileNamesKey = TEXT("ProfileNames");
static const TCHAR* GActiveProfileKey = TEXT("ActiveProfile");

// These settings live in a standalone ini (Config/RPDAssetAuditor.ini) outside UE's
// config hierarchy. GConfig cannot persist such a file: SetString() silently bails when
// the file isn't already loaded (so it is never created), and any file GConfig auto-loads
// via Find() is flagged NoSave and skipped by Flush(). We therefore read/write an
// FConfigFile directly — Write() always hits disk — which is what actually persists.
static void ReadAuditConfigFile(FConfigFile& OutFile)
{
	const FString Path = AuditRules::GetConfigPath();
	if (FPaths::FileExists(Path))
		OutFile.Read(Path);
}

static void WriteAuditConfigFile(FConfigFile& InFile)
{
	InFile.Write(AuditRules::GetConfigPath());
}

// Persist which profile is active so it can be restored on reopen / editor restart.
static void PersistActiveProfile(const FString& ProfileName)
{
	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile);
	ConfigFile.SetString(GProfileNamesSection, GActiveProfileKey, *ProfileName);
	WriteAuditConfigFile(ConfigFile);
}

void AuditRules::ScanProfiles()
{
	ProfileList.Empty();
	ProfileList.Add(MakeShareable(new FString(TEXT("_default"))));

	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile);

	TArray<FString> Names;
	ConfigFile.GetArray(GProfileNamesSection, GProfileNamesKey, Names);
	for (const FString& Name : Names)
	{
		if (!Name.IsEmpty() && Name != TEXT("_default"))
			ProfileList.AddUnique(MakeShareable(new FString(Name)));
	}
}

void AuditRules::SaveProfile(const FString& ProfileName)
{
	if (ProfileName.IsEmpty() || ProfileName == TEXT("_default"))
		return;

	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile); // preserve the main section and other profiles

	const FString Section = TEXT("Profile ") + ProfileName;

	ConfigFile.SetBool(*Section, TEXT("bNaniteInverted"), bNaniteInverted);

	TArray<FString> ThresholdKeys;
	RuleConfig.GetKeys(ThresholdKeys);
	ConfigFile.SetArray(*Section, TEXT("ThresholdKeys"), ThresholdKeys);
	for (const auto& KVP : RuleConfig)
		ConfigFile.SetInt64(*Section, *KVP.Key, KVP.Value);

	TArray<FString> RuleKeys;
	RuleEnabled.GetKeys(RuleKeys);
	ConfigFile.SetArray(*Section, TEXT("RuleKeys"), RuleKeys);
	for (const auto& KVP : RuleEnabled)
		ConfigFile.SetBool(*Section, *KVP.Key, KVP.Value);

	// Register the profile name so ScanProfiles can rediscover it next session.
	TArray<FString> Names;
	ConfigFile.GetArray(GProfileNamesSection, GProfileNamesKey, Names);
	Names.AddUnique(ProfileName);
	ConfigFile.SetArray(GProfileNamesSection, GProfileNamesKey, Names);

	// Saving makes this the active profile so reopening restores it (same write).
	ConfigFile.SetString(GProfileNamesSection, GActiveProfileKey, *ProfileName);

	WriteAuditConfigFile(ConfigFile);

	CurrentProfile = ProfileName;
	ScanProfiles();
}

void AuditRules::LoadProfile(const FString& ProfileName)
{
	if (ProfileName.IsEmpty())
		return;

	if (ProfileName == TEXT("_default"))
	{
		ResetRuleConfig();
		CurrentProfile = ProfileName;
		PersistActiveProfile(ProfileName);
		return;
	}

	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile);

	const FString Section = TEXT("Profile ") + ProfileName;

	ConfigFile.GetBool(*Section, TEXT("bNaniteInverted"), bNaniteInverted);

	RuleConfig.Empty();
	TArray<FString> ThresholdKeys;
	ConfigFile.GetArray(*Section, TEXT("ThresholdKeys"), ThresholdKeys);
	for (const FString& Key : ThresholdKeys)
	{
		int32 Val = 0;
		if (ConfigFile.GetInt(*Section, *Key, Val))
			RuleConfig.Add(Key, Val);
	}

	RuleEnabled.Empty();
	TArray<FString> RuleKeys;
	ConfigFile.GetArray(*Section, TEXT("RuleKeys"), RuleKeys);
	for (const FString& Key : RuleKeys)
	{
		bool bVal = true;
		if (ConfigFile.GetBool(*Section, *Key, bVal))
			RuleEnabled.Add(Key, bVal);
	}

	CurrentProfile = ProfileName;
	PersistActiveProfile(ProfileName);
}

void AuditRules::DeleteProfile(const FString& ProfileName)
{
	if (ProfileName.IsEmpty() || ProfileName == TEXT("_default"))
		return;

	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile);

	// Drop the name from the tracked profile list. The orphaned "Profile <name>"
	// section is harmless — ScanProfiles only surfaces names listed in this array.
	TArray<FString> Names;
	ConfigFile.GetArray(GProfileNamesSection, GProfileNamesKey, Names);
	Names.Remove(ProfileName);
	ConfigFile.SetArray(GProfileNamesSection, GProfileNamesKey, Names);

	if (CurrentProfile == ProfileName)
	{
		CurrentProfile = TEXT("_default");
		ConfigFile.SetString(GProfileNamesSection, GActiveProfileKey, TEXT("_default"));
	}

	WriteAuditConfigFile(ConfigFile);
	ScanProfiles();
}

void AuditRules::LoadConfig()
{
	const FString ConfigPath = GetConfigPath();
	if (!FPaths::FileExists(ConfigPath))
	{
		ResetRuleConfig();
		return;
	}

	FConfigFile ConfigFile;
	ConfigFile.Read(ConfigPath);

	const FString Section = TEXT("RPDAssetAuditor");

	// Load bNaniteInverted
	ConfigFile.GetBool(*Section, TEXT("bNaniteInverted"), bNaniteInverted);

	// Load RuleConfig thresholds
	TArray<FString> ThresholdKeys;
	ConfigFile.GetArray(*Section, TEXT("ThresholdKeys"), ThresholdKeys);
	for (const FString& Key : ThresholdKeys)
	{
		int32 Val = 0;
		if (ConfigFile.GetInt(*Section, *Key, Val))
		{
			RuleConfig.Add(Key, Val);
		}
	}

	// Load RuleEnabled toggles
	TArray<FString> RuleKeys;
	ConfigFile.GetArray(*Section, TEXT("RuleKeys"), RuleKeys);
	for (const FString& Key : RuleKeys)
	{
		bool bVal = true;
		if (ConfigFile.GetBool(*Section, *Key, bVal))
		{
			RuleEnabled.Add(Key, bVal);
		}
	}

	// Load NamingPrefixes
	NamingPrefixes.Empty();
	TArray<FString> PrefixKeys;
	ConfigFile.GetArray(*Section, TEXT("PrefixKeys"), PrefixKeys);
	for (const FString& Key : PrefixKeys)
	{
		FString Val;
		if (ConfigFile.GetString(*Section, *Key, Val))
			NamingPrefixes.Add(Key, Val);
	}
	ConfigFile.GetArray(*Section, TEXT("IgnoredPaths"), IgnoredAssetPaths);

	// If a named profile was active, overlay its settings so it is live on startup.
	FString ActiveProfile;
	ConfigFile.GetString(GProfileNamesSection, GActiveProfileKey, ActiveProfile);
	if (!ActiveProfile.IsEmpty() && ActiveProfile != TEXT("_default"))
	{
		LoadProfile(ActiveProfile);
	}
	else
	{
		CurrentProfile = TEXT("_default");
	}
}

void AuditRules::SaveConfig()
{
	FConfigFile ConfigFile;
	ReadAuditConfigFile(ConfigFile); // preserve profile sections / active-profile marker

	const FString Section = TEXT("RPDAssetAuditor");

	// Save bNaniteInverted
	ConfigFile.SetBool(*Section, TEXT("bNaniteInverted"), bNaniteInverted);

	// Save RuleConfig. (Stale individual keys are harmless — reads go through the
	// ThresholdKeys/RuleKeys/PrefixKeys arrays, which are rewritten in full here.)
	TArray<FString> ThresholdKeys;
	RuleConfig.GetKeys(ThresholdKeys);
	ConfigFile.SetArray(*Section, TEXT("ThresholdKeys"), ThresholdKeys);
	for (const auto& KVP : RuleConfig)
	{
		ConfigFile.SetInt64(*Section, *KVP.Key, KVP.Value);
	}

	// Save RuleEnabled
	TArray<FString> RuleKeys;
	RuleEnabled.GetKeys(RuleKeys);
	ConfigFile.SetArray(*Section, TEXT("RuleKeys"), RuleKeys);
	for (const auto& KVP : RuleEnabled)
	{
		ConfigFile.SetBool(*Section, *KVP.Key, KVP.Value);
	}

	// Save NamingPrefixes
	TArray<FString> PrefixKeys;
	NamingPrefixes.GetKeys(PrefixKeys);
	ConfigFile.SetArray(*Section, TEXT("PrefixKeys"), PrefixKeys);
	for (const auto& KVP : NamingPrefixes)
		ConfigFile.SetString(*Section, *KVP.Key, *KVP.Value);
	ConfigFile.SetArray(*Section, TEXT("IgnoredPaths"), IgnoredAssetPaths);

	WriteAuditConfigFile(ConfigFile);
}

void AuditRules::RegisterBuiltInRules()
{
	FRPDAuditRuleRegistry& Registry = FRPDAuditRuleRegistry::Get();

	// Static Mesh
	Registry.RegisterRule(Rule_NoLODs);
	Registry.RegisterRule(Rule_NoLODsAndNaniteDisabled);
	Registry.RegisterRule(Rule_HighVertexCount);
	Registry.RegisterRule(Rule_HighMaterialCount);
	Registry.RegisterRule(Rule_HighUVChannelCount);
	Registry.RegisterRule(Rule_MissingCollision);
	Registry.RegisterRule(Rule_MissingStaticMeshMaterial);
	Registry.RegisterRule(Rule_SameMaterialMultipleSlots);

	// Skeletal Mesh
	Registry.RegisterRule(Rule_SkeletalMesh_NoLODs);
	Registry.RegisterRule(Rule_SkeletalMesh_HighVertexCount);
	Registry.RegisterRule(Rule_SkeletalMesh_HighMaterialCount);
	Registry.RegisterRule(Rule_MissingPhysicsAsset);
	Registry.RegisterRule(Rule_MissingSkeletalMeshMaterial);

	// Texture
	Registry.RegisterRule(Rule_NonPowerOfTwo);
	Registry.RegisterRule(Rule_LargeTexture);
	Registry.RegisterRule(Rule_VirtualTextureMismatch);
	Registry.RegisterRule(Rule_NeverStream);
	Registry.RegisterRule(Rule_TextureLODGroup);

	// Sound
	Registry.RegisterRule(Rule_MissingSoundClass);
	Registry.RegisterRule(Rule_MissingAttenuation);
	Registry.RegisterRule(Rule_AudioCompression);
	Registry.RegisterRule(Rule_WAVLength);
	Registry.RegisterRule(Rule_NoConcurrencySettings);

	// Blueprint
	Registry.RegisterRule(Rule_BlueprintComplexParents);
	Registry.RegisterRule(Rule_BlueprintComplexity);
	Registry.RegisterRule(Rule_BlueprintExposedParams);
	Registry.RegisterRule(Rule_WidgetBlueprintComplexity);

	// Misc
	Registry.RegisterRule(Rule_UnusedAsset);
	Registry.RegisterRule(Rule_ProjectRedirectors);

	// Animation
	Registry.RegisterRule(Rule_LongAnimSequence);

	// Material / Material Instance
	Registry.RegisterRule(Rule_MasterMaterialBloat);
	Registry.RegisterRule(Rule_MaterialInstanceBloat);
	Registry.RegisterRule(Rule_MaterialInstanceMinimalOverrides);
	Registry.RegisterRule(Rule_ListAllMasterMaterials);
	Registry.RegisterRule(Rule_BlendModeMismatch);
	Registry.RegisterRule(Rule_ShadingModelMismatch);
	Registry.RegisterRule(Rule_HighTextureSampling);
	Registry.RegisterRule(Rule_OneOffMaterial);
	Registry.RegisterRule(Rule_DuplicateMaterialInstance);
	Registry.RegisterRule(Rule_DirectMasterMaterialOnStaticMesh);
	Registry.RegisterRule(Rule_DirectMasterMaterialOnSkeletalMesh);
	Registry.RegisterRule(Rule_UnusedMasterParameters);
}
