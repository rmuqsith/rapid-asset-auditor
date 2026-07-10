// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "RPDAuditResult.h"
#include "RPDAuditRules.h"
#include "Containers/Map.h"

// UE version compatibility
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
	#define RPD_USE_TOPLEVELASSETPATH 1
#else
	#define RPD_USE_TOPLEVELASSETPATH 0
#endif

/**
 * Optional material-domain filter for a naming convention. Regular and
 * post-process materials share the same UClass (UMaterial), so a naming rule
 * that should apply to only one of them uses this to disambiguate by domain.
 */
enum class ERPDMaterialDomainFilter : uint8
{
	Any,          // no domain filtering (applies to all classes)
	SurfaceOnly,  // regular materials only — excludes post-process
	PostProcess   // post-process materials only
};

/** Naming convention entry: prefix + asset type filter */
struct FRPDNamingConvention
{
	FString Prefix;
	FString Suffix;
	FName TargetClassName; // e.g. UStaticMesh::StaticClass()->GetFName()
	ERPDMaterialDomainFilter DomainFilter = ERPDMaterialDomainFilter::Any;
};

/**
 * Main orchestrator for asset auditing.
 * Scans assets via the Asset Registry and runs audit rules against them.
 */
class FRPDAssetAuditManager
{
public:
	FRPDAssetAuditManager();
	~FRPDAssetAuditManager();

	/** Run a full audit on the project (or a given path) */
	FRPDAuditResult RunAudit(const FString& RootPath = TEXT("/Game"), bool bRecursive = true);

	/** Run audit only for specific asset classes (pass FTopLevelAssetPath, e.g. UStaticMesh::StaticClass()->GetClassPathName()) */
	#if RPD_USE_TOPLEVELASSETPATH
	FRPDAuditResult RunAuditForClasses(const TArray<FTopLevelAssetPath>& ClassPaths, const FString& RootPath = TEXT("/Game"), bool bRecursive = true);
#else
	FRPDAuditResult RunAuditForClasses(const TArray<FName>& ClassPaths, const FString& RootPath = TEXT("/Game"), bool bRecursive = true);
#endif

	/** List every asset matching the filter as an Info issue (no rules applied) */
	#if RPD_USE_TOPLEVELASSETPATH
	FRPDAuditResult ListAssets(const TArray<FTopLevelAssetPath>& ClassPaths, const FString& RootPath = TEXT("/Game"), bool bRecursive = true);
#else
	FRPDAuditResult ListAssets(const TArray<FName>& ClassPaths, const FString& RootPath = TEXT("/Game"), bool bRecursive = true);
#endif

	/** Set naming conventions to check */
	void SetNamingConventions(const TArray<FRPDNamingConvention>& Conventions);

	/** Export last result to JSON string */
	FString ExportToJSON(const FRPDAuditResult& Result) const;

	/** Export last result to JSON file */
	bool ExportToJSONFile(const FRPDAuditResult& Result, const FString& FilePath) const;

	/** Get the health score from severity-weighted calculation */
	static float CalculateHealthScore(const FRPDAuditResult& Result);

	/** Delegate fired when audit completes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAuditComplete, const FRPDAuditResult&);
	FOnAuditComplete& OnAuditComplete() { return AuditCompleteDelegate; }

private:
	/** Gather assets from the registry using a filter */
	TArray<FAssetData> GatherAssets(const FARFilter& Filter) const;

	/** Run all applicable rules against a single asset */
	void AuditSingleAsset(const FAssetData& AssetData, FRPDAuditResult& OutResult);

	/** Audit every asset with a cancellable per-asset progress dialog. Sets OutResult.bCancelled if cancelled. */
	void AuditAssetsWithProgress(const TArray<FAssetData>& Assets, FRPDAuditResult& OutResult);

	/** Run naming convention checks against a single asset */
	void CheckNamingConvention(const FAssetData& AssetData, FRPDAuditResult& OutResult, const FString& MICParentName = TEXT(""), const FString& MICParentPath = TEXT(""));

	/** Count how many UMaterialInstanceConstant assets reference this material */
	int32 CountMaterialChildren(const FAssetData& AssetData) const;

	/** Active naming conventions */
	TArray<FRPDNamingConvention> NamingConventions;

	/** Delegate instance */
	FOnAuditComplete AuditCompleteDelegate;
};
