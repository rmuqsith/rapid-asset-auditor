// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/NoExportTypes.h"
#include "RPDAuditResult.generated.h"

/** Severity level for an audit issue */
UENUM(BlueprintType)
enum class ERPDAuditSeverity : uint8
{
	Info		UMETA(DisplayName = "Info"),
	Warning		UMETA(DisplayName = "Warning"),
	Critical	UMETA(DisplayName = "Critical"),
	Error		UMETA(DisplayName = "Error")
};

/** A single issue found during an audit scan */
USTRUCT(BlueprintType)
struct FRPDAuditIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString AssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString AssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString ParentName;

	/** Full object path of the parent asset (e.g. material instance's master), for CB navigation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString ParentPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString AssetType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString RuleName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	ERPDAuditSeverity Severity = ERPDAuditSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 SeverityScore = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString Detail;

	/** Number of child material instances (for master materials, -1 = N/A) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 ChildCount = -1;

	/** The raw asset data for UI actions (e.g. sync in content browser) */
	FAssetData AssetData;

	/** Folder portion of the asset path, e.g. "/Game/Materials" (asset name stripped). */
	FString GetAssetFolder() const
	{
		// AssetPath is an object path like "/Game/Materials/M_Foo.M_Foo".
		FString PackageName = AssetPath;
		int32 DotIdx;
		if (PackageName.FindChar(TEXT('.'), DotIdx))
		{
			PackageName = PackageName.Left(DotIdx);
		}
		int32 SlashIdx;
		if (PackageName.FindLastChar(TEXT('/'), SlashIdx))
		{
			return PackageName.Left(SlashIdx);
		}
		return PackageName;
	}
};

/** Results of a single audit run */
USTRUCT(BlueprintType)
struct FRPDAuditResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FString ProjectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	FDateTime ScanDate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	float HealthScore = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 TotalIssues = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 CriticalCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 WarningCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 InfoCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	TArray<FRPDAuditIssue> Issues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	int32 ScannedAssetCount = 0;

	/** True if the user cancelled the scan partway — Issues holds only what was scanned so far. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
	bool bCancelled = false;
};
