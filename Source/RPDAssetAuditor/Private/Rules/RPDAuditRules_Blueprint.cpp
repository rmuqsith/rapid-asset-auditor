// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "Misc/PackageName.h"

#include "RPDAuditRules.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

// NOTE: off by default. Character/Pawn ARE the correct bases for gameplay actors, so
// judging "unnecessary complexity" from the base-class name alone is unreliable — this
// is only a rough inventory heads-up, not a defect. Kept opt-in for that narrow use.
FRPDAuditRule AuditRules::Rule_BlueprintComplexParents = {
	TEXT("BlueprintComplexParents"),
	TEXT("Lists Blueprints deriving from Character/Pawn (inventory only — these are the correct bases for gameplay actors, not a problem). Off by default."),
	UBlueprint::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 1,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		FString ParentClassStr;
		if (AssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassStr))
		{
			FString CleanParent = FPackageName::ExportTextPathToObjectPath(ParentClassStr);
			if (!CleanParent.IsEmpty() && (CleanParent.Contains(TEXT("Character")) || CleanParent.Contains(TEXT("Pawn"))))
			{
				OutIssue.Detail = FString::Printf(TEXT("Derives from %s (a standard gameplay base — informational only)."), *CleanParent);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_BlueprintComplexity = {
	TEXT("BlueprintComplexity"),
	TEXT("Blueprint has high script complexity — many functions or variables may impact performance."),
	UBlueprint::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		FString NumFuncs, NumVars;
		AssetData.GetTagValue(TEXT("NumFunctions"), NumFuncs);
		AssetData.GetTagValue(TEXT("NumVariables"), NumVars);

		int32 FuncCount = FCString::Atoi(*NumFuncs);
		int32 VarCount = FCString::Atoi(*NumVars);

		if (FuncCount > 30)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d functions — consider splitting into subsystems."), FuncCount);
			return true;
		}
		if (VarCount > 40)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d variables — consider grouping related variables into structs."), VarCount);
			OutIssue.SeverityScore = 2;
			return true;
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_BlueprintExposedParams = {
	TEXT("BlueprintExposedParams"),
	TEXT("Blueprint exposes many parameters to instances — may increase complexity."),
	UBlueprint::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		FString NumParams;
		AssetData.GetTagValue(TEXT("NumExposedVariables"), NumParams);
		int32 ParamCount = FCString::Atoi(*NumParams);
		if (ParamCount > 15)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d exposed parameters — consider reducing public interface."), ParamCount);
			return true;
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_WidgetBlueprintComplexity = {
	TEXT("WidgetBlueprintComplexity"),
	TEXT("Widget Blueprint has many widgets or bindings — may impact UI performance."),
	// UWidgetBlueprint is in UMG module; we use a well-known class path to avoid hard dependency
	FName(TEXT("/Script/UMGEditor.WidgetBlueprint")),
	ERPDAuditSeverity::Warning, 4,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		FString NumWidgets, NumBindings;
		AssetData.GetTagValue(TEXT("NumWidgets"), NumWidgets);
		AssetData.GetTagValue(TEXT("NumBindings"), NumBindings);

		int32 WidgetCount = FCString::Atoi(*NumWidgets);
		int32 BindingCount = FCString::Atoi(*NumBindings);

		if (WidgetCount > 30)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d widgets — very complex widget tree, consider nesting limits."), WidgetCount);
			return true;
		}
		if (BindingCount > 10)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d bindings — each binding evaluates per tick, consider caching."), BindingCount);
			return true;
		}
		return false;
	}
};
