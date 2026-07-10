// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAssetAuditManager.h"

// UE version compatibility: FTopLevelAssetPath was introduced in UE 5.3
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3)
	typedef FName FTopLevelAssetPath;
#endif
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "Engine/ObjectLibrary.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWave.h"
#include "Engine/Blueprint.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "MaterialDomain.h" // EMaterialDomain / MD_PostProcess
#include "Misc/App.h"
#include "Misc/ScopedSlowTask.h"

FRPDAssetAuditManager::FRPDAssetAuditManager()
{
	// Ensure built-in rules are registered
	AuditRules::RegisterBuiltInRules();
}

FRPDAssetAuditManager::~FRPDAssetAuditManager()
{
}

// Adds one or more package paths to the filter. RootPath may be a single path
// ("/Game/Foo") or a comma-separated list ("/Game/Foo, /Game/Bar") as produced by
// the multi-folder context-menu action. A comma-joined string is not a valid
// package path, so it must be split.
static void AddPackagePathsToFilter(FARFilter& Filter, const FString& RootPath)
{
	TArray<FString> Paths;
	RootPath.ParseIntoArray(Paths, TEXT(","), /*bCullEmpty=*/true);

	bool bAddedAny = false;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		if (!Path.IsEmpty())
		{
			Filter.PackagePaths.Add(*Path);
			bAddedAny = true;
		}
	}

	// Fallback: if parsing yielded nothing usable, use the raw string as-is.
	if (!bAddedAny)
	{
		AddPackagePathsToFilter(Filter, RootPath);
	}
}

TArray<FAssetData> FRPDAssetAuditManager::GatherAssets(const FARFilter& Filter) const
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);
	return AssetList;
}

void FRPDAssetAuditManager::AuditSingleAsset(const FAssetData& AssetData, FRPDAuditResult& OutResult)
{
	if (!AssetData.IsValid())
		return;

	// Skip ignored assets
	if (AuditRules::IsAssetIgnored(AssetData.GetObjectPathString()))
		return;

	FName ClassName = AssetData.AssetClassPath.GetAssetName();

	// ── Pre-load parent name for Material Instance Constants ──────────────
	// Populated once here so every rule (and the naming check) gets it
	// without each rule having to remember to set ParentName.
	FString MICParentName;
	FString MICParentPath;
	if (ClassName == UMaterialInstanceConstant::StaticClass()->GetFName())
	{
		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			if (MIC->Parent)
			{
				MICParentName = MIC->Parent->GetName();
				MICParentPath = MIC->Parent->GetPathName();
			}
		}
	}

	// Get rules applicable to this class (and generic rules with NAME_None)
	TArray<FRPDAuditRule> ApplicableRules;
	for (const FRPDAuditRule& Rule : FRPDAuditRuleRegistry::Get().GetAllRules())
	{
		// Skip if rule is disabled in config
		bool* bEnabled = AuditRules::RuleEnabled.Find(Rule.RuleName);
		if (bEnabled && !(*bEnabled))
			continue;

		if (Rule.AssetClassName == ClassName || Rule.AssetClassName == NAME_None)
		{
			ApplicableRules.Add(Rule);
		}
	}

	int32 ChildCount = -1;
	if (ClassName == UMaterial::StaticClass()->GetFName())
	{
		ChildCount = CountMaterialChildren(AssetData);
	}

	for (const FRPDAuditRule& Rule : ApplicableRules)
	{
		FRPDAuditIssue Issue;
		// Seed defaults BEFORE running the check so a rule's lambda can override the
		// per-issue severity/score (e.g. a warn-tier vs. critical-tier hit within the
		// same rule) and have that value survive.
		Issue.Severity = Rule.DefaultSeverity;
		Issue.SeverityScore = Rule.SeverityScore;
		// Seed the child (instance) count BEFORE the check so a rule can key off reuse
		// (e.g. the master-material consolidation signal), then keep it on the issue.
		Issue.ChildCount = ChildCount;
		bool bHasIssue = Rule.CheckFunc(AssetData, Issue);
		if (bHasIssue)
		{
			Issue.AssetPath = AssetData.GetObjectPathString();
			Issue.AssetName = AssetData.AssetName.ToString();
			Issue.AssetType = ClassName.ToString();
			Issue.RuleName = Rule.RuleName;
			Issue.AssetData = AssetData;

			// Apply MIC parent name if the rule didn't already set it
			if (!MICParentName.IsEmpty() && Issue.ParentName.IsEmpty())
			{
				Issue.ParentName = MICParentName;
			}
			// Always record the resolved parent object path so the Parent column
			// can navigate to the actual master asset.
			if (!MICParentPath.IsEmpty() && Issue.ParentPath.IsEmpty())
			{
				Issue.ParentPath = MICParentPath;
			}

			OutResult.Issues.Add(Issue);
			OutResult.TotalIssues++;

			switch (Issue.Severity)
			{
			case ERPDAuditSeverity::Critical:
			case ERPDAuditSeverity::Error:
				OutResult.CriticalCount++;
				break;
			case ERPDAuditSeverity::Warning:
				OutResult.WarningCount++;
				break;
			default:
				OutResult.InfoCount++;
				break;
			}
		}
	}

	// Naming convention check
	CheckNamingConvention(AssetData, OutResult, MICParentName, MICParentPath);
}

void FRPDAssetAuditManager::CheckNamingConvention(const FAssetData& AssetData, FRPDAuditResult& OutResult, const FString& MICParentName, const FString& MICParentPath)
{
	if (NamingConventions.Num() == 0)
		return;

	FName ClassName = AssetData.AssetClassPath.GetAssetName();
	FString AssetName = AssetData.AssetName.ToString();

	for (const FRPDNamingConvention& Convention : NamingConventions)
	{
		// Skip if this convention targets a different class
		if (Convention.TargetClassName != NAME_None && Convention.TargetClassName != ClassName)
			continue;

		// Regular and post-process materials are both UMaterial, so split them by
		// material domain when a convention only applies to one of them.
		if (Convention.DomainFilter != ERPDMaterialDomainFilter::Any)
		{
			const UMaterial* Mat = Cast<UMaterial>(AssetData.GetAsset());
			const bool bIsPostProcess = Mat && Mat->MaterialDomain == MD_PostProcess;
			if (Convention.DomainFilter == ERPDMaterialDomainFilter::PostProcess && !bIsPostProcess)
				continue;
			if (Convention.DomainFilter == ERPDMaterialDomainFilter::SurfaceOnly && bIsPostProcess)
				continue;
		}

		// Human-readable asset kind for the message (e.g. "post-process material").
		const FString AssetKind =
			(Convention.DomainFilter == ERPDMaterialDomainFilter::PostProcess) ? TEXT("post-process material") :
			(Convention.DomainFilter == ERPDMaterialDomainFilter::SurfaceOnly) ? TEXT("material") :
			ClassName.ToString();

		// Check prefix
		if (!Convention.Prefix.IsEmpty() && !AssetName.StartsWith(Convention.Prefix, ESearchCase::IgnoreCase))
		{
			FRPDAuditIssue Issue;
			Issue.AssetPath = AssetData.GetObjectPathString();
			Issue.AssetName = AssetName;
			Issue.AssetType = ClassName.ToString();
			Issue.ParentName = MICParentName;
			Issue.ParentPath = MICParentPath;
			Issue.RuleName = FString::Printf(TEXT("Naming_%s"), *Convention.Prefix);
			Issue.Severity = ERPDAuditSeverity::Warning;
			Issue.SeverityScore = 3;
			Issue.Detail = FString::Printf(TEXT("Expected %s prefix '%s' but found '%s'."), *AssetKind, *Convention.Prefix, *AssetName);
			Issue.AssetData = AssetData;

			OutResult.Issues.Add(Issue);
			OutResult.TotalIssues++;
			OutResult.WarningCount++;
			return; // Only report one naming issue per asset
		}

		// Check suffix
		if (!Convention.Suffix.IsEmpty() && !AssetName.EndsWith(Convention.Suffix))
		{
			FRPDAuditIssue Issue;
			Issue.AssetPath = AssetData.GetObjectPathString();
			Issue.AssetName = AssetName;
			Issue.AssetType = ClassName.ToString();
			Issue.ParentName = MICParentName;
			Issue.ParentPath = MICParentPath;
			Issue.RuleName = FString::Printf(TEXT("Naming_%s"), *Convention.Suffix);
			Issue.Severity = ERPDAuditSeverity::Warning;
			Issue.SeverityScore = 3;
			Issue.Detail = FString::Printf(TEXT("Expected suffix '%s' but found '%s'."), *Convention.Suffix, *AssetName);
			Issue.AssetData = AssetData;

			OutResult.Issues.Add(Issue);
			OutResult.TotalIssues++;
			OutResult.WarningCount++;
			return;
		}
	}
}

void FRPDAssetAuditManager::SetNamingConventions(const TArray<FRPDNamingConvention>& Conventions)
{
	NamingConventions = Conventions;
}

FRPDAuditResult FRPDAssetAuditManager::RunAudit(const FString& RootPath, bool bRecursive)
{
	FRPDAuditResult Result;
	Result.ProjectName = FApp::GetProjectName();
	Result.ScanDate = FDateTime::UtcNow();

	// Build a filter for all known asset classes
	FARFilter Filter;
	AddPackagePathsToFilter(Filter, RootPath);
	Filter.bRecursivePaths = bRecursive;

	// Include common asset types
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());

	TArray<FAssetData> AllAssets = GatherAssets(Filter);
	AllAssets.Shrink();

	AuditAssetsWithProgress(AllAssets, Result);

	// Calculate health score
	Result.HealthScore = CalculateHealthScore(Result);

	// Broadcast completion
	AuditCompleteDelegate.Broadcast(Result);

	return Result;
}

FRPDAuditResult FRPDAssetAuditManager::RunAuditForClasses(const TArray<FTopLevelAssetPath>& ClassPaths, const FString& RootPath, bool bRecursive)
{
	FRPDAuditResult Result;
	Result.ProjectName = FApp::GetProjectName();
	Result.ScanDate = FDateTime::UtcNow();

	FARFilter Filter;
	AddPackagePathsToFilter(Filter, RootPath);
	Filter.bRecursivePaths = bRecursive;

	Filter.ClassPaths = ClassPaths;

	TArray<FAssetData> AllAssets = GatherAssets(Filter);
	AllAssets.Shrink();

	AuditAssetsWithProgress(AllAssets, Result);

	Result.HealthScore = CalculateHealthScore(Result);
	AuditCompleteDelegate.Broadcast(Result);

	return Result;
}

void FRPDAssetAuditManager::AuditAssetsWithProgress(const TArray<FAssetData>& Assets, FRPDAuditResult& OutResult)
{
	OutResult.ScannedAssetCount = Assets.Num();

	// Cancellable dialog that ticks once per asset, so a large scan shows real
	// progress and can be aborted instead of freezing the editor on a static dialog.
	FScopedSlowTask SlowTask(static_cast<float>(Assets.Num()), FText::FromString(TEXT("Scanning assets...")));
	SlowTask.MakeDialog(true /*bShowCancelButton*/);

	for (const FAssetData& AssetData : Assets)
	{
		if (SlowTask.ShouldCancel())
		{
			OutResult.bCancelled = true;
			break;
		}
		SlowTask.EnterProgressFrame(1.0f,
			FText::FromString(FString::Printf(TEXT("Auditing %s"), *AssetData.AssetName.ToString())));
		AuditSingleAsset(AssetData, OutResult);
	}
}

float FRPDAssetAuditManager::CalculateHealthScore(const FRPDAuditResult& Result)
{
	// Score = (clean assets / total scanned) × 100
	// An issue-free project = 100. Every asset with at least one issue reduces the score.
	// If the scan was cancelled, ScannedAssetCount is still the full total, which is misleading,
	// so score is 0.
	if (Result.bCancelled || Result.ScannedAssetCount == 0)
		return 0.0f;

	// Count unique assets that have at least one non-Info issue
	// (Info is advisory, not a defect)
	TSet<FString> ProblemAssets;
	for (const FRPDAuditIssue& Issue : Result.Issues)
	{
		if (Issue.Severity != ERPDAuditSeverity::Info)
			ProblemAssets.Add(Issue.AssetPath);
	}

	const int32 CleanAssets = Result.ScannedAssetCount - ProblemAssets.Num();
	const float Score = (static_cast<float>(CleanAssets) / Result.ScannedAssetCount) * 100.0f;
	return FMath::Clamp(Score, 0.0f, 100.0f);
}

int32 FRPDAssetAuditManager::CountMaterialChildren(const FAssetData& AssetData) const
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetIdentifier> Referencers;
	Registry.GetReferencers(AssetData.PackageName, Referencers);
	// Filter down to only UMaterialInstanceConstant referencers in the same content
	int32 Count = 0;
	for (const FAssetIdentifier& Ref : Referencers)
	{
		TArray<FAssetData> RefAssets;
		Registry.GetAssetsByPackageName(Ref.PackageName, RefAssets);
		for (const FAssetData& RefAsset : RefAssets)
		{
			if (RefAsset.GetClass() == UMaterialInstanceConstant::StaticClass())
			{
				Count++;
			}
		}
	}
	return Count;
}

FRPDAuditResult FRPDAssetAuditManager::ListAssets(const TArray<FTopLevelAssetPath>& ClassPaths, const FString& RootPath, bool bRecursive)
{
	FRPDAuditResult Result;
	Result.ProjectName = FApp::GetProjectName();
	Result.ScanDate = FDateTime::UtcNow();

	FARFilter Filter;
	AddPackagePathsToFilter(Filter, RootPath);
	Filter.bRecursivePaths = bRecursive;
	Filter.ClassPaths = ClassPaths;

	TArray<FAssetData> AllAssets = GatherAssets(Filter);
	AllAssets.Shrink();

	for (const FAssetData& AssetData : AllAssets)
	{
		if (!AssetData.IsValid())
			continue;

		FRPDAuditIssue Issue;
		Issue.AssetPath = AssetData.GetObjectPathString();
		Issue.AssetName = AssetData.AssetName.ToString();
		Issue.AssetType = AssetData.AssetClassPath.GetAssetName().ToString();
		Issue.RuleName = TEXT("Asset Listing");
		Issue.Severity = ERPDAuditSeverity::Info;
		Issue.SeverityScore = 0;
		Issue.AssetData = AssetData;
		Issue.Detail = FString();

		// Count children for master materials
		if (AssetData.GetClass() == UMaterial::StaticClass())
		{
			Issue.ChildCount = CountMaterialChildren(AssetData);
		}

		Result.Issues.Add(Issue);
		Result.TotalIssues++;
		Result.InfoCount++;
	}

	Result.HealthScore = 0.0f;
	AuditCompleteDelegate.Broadcast(Result);

	return Result;
}

FString FRPDAssetAuditManager::ExportToJSON(const FRPDAuditResult& Result) const
{
	TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);
	RootObj->SetStringField(TEXT("projectName"), Result.ProjectName);
	RootObj->SetStringField(TEXT("scanDate"), Result.ScanDate.ToIso8601());
	RootObj->SetNumberField(TEXT("healthScore"), Result.HealthScore);
	RootObj->SetNumberField(TEXT("totalIssues"), Result.TotalIssues);
	RootObj->SetNumberField(TEXT("critical"), Result.CriticalCount);
	RootObj->SetNumberField(TEXT("warning"), Result.WarningCount);
	RootObj->SetNumberField(TEXT("info"), Result.InfoCount);

	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	for (const FRPDAuditIssue& Issue : Result.Issues)
	{
		TSharedPtr<FJsonObject> IssueObj = MakeShareable(new FJsonObject);
		IssueObj->SetStringField(TEXT("assetPath"), Issue.AssetPath);
		IssueObj->SetStringField(TEXT("assetName"), Issue.AssetName);
		IssueObj->SetStringField(TEXT("assetType"), Issue.AssetType);
		IssueObj->SetStringField(TEXT("rule"), Issue.RuleName);
		IssueObj->SetStringField(TEXT("severity"), StaticEnum<ERPDAuditSeverity>()->GetNameStringByValue((int64)Issue.Severity));
		IssueObj->SetNumberField(TEXT("severityScore"), Issue.SeverityScore);
		IssueObj->SetStringField(TEXT("parentName"), Issue.ParentName);
		IssueObj->SetStringField(TEXT("parentPath"), Issue.ParentPath);
		IssueObj->SetNumberField(TEXT("childCount"), Issue.ChildCount);
		IssueObj->SetStringField(TEXT("detail"), Issue.Detail);
		IssuesArray.Add(MakeShareable(new FJsonValueObject(IssueObj)));
	}
	RootObj->SetArrayField(TEXT("issues"), IssuesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);

	return OutputString;
}

bool FRPDAssetAuditManager::ExportToJSONFile(const FRPDAuditResult& Result, const FString& FilePath) const
{
	FString JSONString = ExportToJSON(Result);
	return FFileHelper::SaveStringToFile(JSONString, *FilePath);
}
