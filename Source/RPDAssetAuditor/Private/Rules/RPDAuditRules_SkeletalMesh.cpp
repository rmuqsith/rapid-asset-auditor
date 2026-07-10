// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"

FRPDAuditRule AuditRules::Rule_SkeletalMesh_NoLODs = {
	TEXT("SkeletalMesh_NoLODs"),
	TEXT("Skeletal mesh has only 1 LOD — no distance-based simplification."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 6,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (Mesh->GetLODNum() <= 1)
			{
				// Gate on the same vertex floor as static-mesh NoLODs — tiny skeletal
				// meshes (simple props/attachments) don't benefit from a LOD chain.
				int32 Verts = 0;
				const FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
				if (ImportedModel && ImportedModel->LODModels.Num() > 0)
					Verts = ImportedModel->LODModels[0].NumVertices;

				if (Verts >= RuleConfig.FindRef(TEXT("NoLODs_MinVerts"), 1000))
				{
					OutIssue.Detail = FString::Printf(TEXT("Skeletal mesh has only 1 LOD. %d verts in LOD 0."), Verts);
					return true;
				}
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_SkeletalMesh_HighVertexCount = {
	TEXT("SkeletalMesh_HighVertexCount"),
	TEXT("Skeletal mesh has high vertex count — potential performance impact."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (Mesh->GetLODNum() > 0)
			{
				int32 Verts = 0;
				const FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
				if (ImportedModel && ImportedModel->LODModels.Num() > 0)
				{
					Verts = ImportedModel->LODModels[0].NumVertices;
				}

				int32 SkelVertWarn = RuleConfig.FindRef(TEXT("SkeletalVertex_Warning"), 10000);
				int32 SkelVertCrit = RuleConfig.FindRef(TEXT("SkeletalVertex_Critical"), 20000);
				if (Verts > SkelVertCrit)
				{
					OutIssue.Detail = FString::Printf(TEXT("%d vertices in LOD 0."), Verts);
					return true;
				}
				if (Verts >= SkelVertWarn)
				{
					OutIssue.Detail = FString::Printf(TEXT("%d vertices — moderately high. Consider LODs for distant views."), Verts);
					OutIssue.SeverityScore = 3;
					return true;
				}
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_SkeletalMesh_HighMaterialCount = {
	TEXT("SkeletalMesh_HighMaterialCount"),
	TEXT("Skeletal mesh uses many material slots."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			int32 MatCount = Mesh->GetMaterials().Num();
			int32 SkelMatWarn = RuleConfig.FindRef(TEXT("SkeletalMatCount_Warning"), 5);
			int32 SkelMatCrit = RuleConfig.FindRef(TEXT("SkeletalMatCount_Critical"), 8);
			if (MatCount > SkelMatCrit)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d material slots — high draw call count."), MatCount);
				return true;
			}
			if (MatCount >= SkelMatWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d material slots — consider combining if possible."), MatCount);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_MissingPhysicsAsset = {
	TEXT("MissingPhysicsAsset"),
	TEXT("Skeletal mesh has no physics asset — can't simulate physics collisions."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 6,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (Mesh->GetPhysicsAsset() == nullptr)
			{
				OutIssue.Detail = TEXT("No physics asset assigned.");
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_MissingSkeletalMeshMaterial = {
	TEXT("MissingSkeletalMeshMaterial"),
	TEXT("Skeletal mesh has empty material slot — will render with missing material."),
	USkeletalMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 7,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			int32 MissingCount = 0;
			for (const FSkeletalMaterial& Mat : Mesh->GetMaterials())
			{
				if (Mat.MaterialInterface == nullptr)
					MissingCount++;
			}
			if (MissingCount > 0)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d empty material slot(s)."), MissingCount);
				return true;
			}
		}
		return false;
	}
};
