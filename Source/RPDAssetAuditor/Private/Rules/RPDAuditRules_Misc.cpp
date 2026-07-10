// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

FRPDAuditRule AuditRules::Rule_UnusedAsset = {
	TEXT("UnusedAsset"),
	TEXT("Asset is not referenced by any other asset in the project — possible orphan."),
	NAME_None,
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// Skip redirectors — they have their own rule
		if (AssetData.IsRedirector())
			return false;

		// Skip root/entry asset types that are legitimately never referenced by
		// other assets (maps and their build data). Flagging these as "unused" is
		// pure noise and previously drove the health score to zero on real projects.
		{
			static const TSet<FName> RootTypes = {
				FName(TEXT("World")),
				FName(TEXT("MapBuildDataRegistry"))
			};
			if (RootTypes.Contains(AssetData.AssetClassPath.GetAssetName()))
				return false;
		}

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetIdentifier> Referencers;
		Registry.GetReferencers(AssetData.PackageName, Referencers);
		if (Referencers.Num() == 0)
		{
			OutIssue.Detail = TEXT("No other assets reference this asset.");
			return true;
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_ProjectRedirectors = {
	TEXT("ProjectRedirectors"),
	TEXT("Redirector asset left behind after moving/renaming — should be cleaned up."),
	NAME_None,
	ERPDAuditSeverity::Warning, 4,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (AssetData.IsRedirector())
		{
			OutIssue.Detail = FString::Printf(
				TEXT("Redirector from '%s'. Clean up via Fix Up Redirectors in Reference Viewer."),
				*AssetData.AssetName.ToString());
			return true;
		}
		return false;
	}
};
