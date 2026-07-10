// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
	// Minimum LOD-0 vertex count before a missing LOD chain is worth flagging.
	// Tiny meshes gain nothing from LODs, so gating here removes the bulk of noise.
	int32 NoLODsMinVerts()
	{
		return AuditRules::RuleConfig.FindRef(TEXT("NoLODs_MinVerts"), 1000);
	}

	// True when a mesh trips the Critical "no LODs + wrong Nanite setting" condition:
	// no LOD chain, and its Nanite flag is the wrong one for the project's mode.
	// The NoLODs (Warning) rule defers to this so a mesh is never counted under both.
	bool IsCriticalNoLODNanite(const UStaticMesh* Mesh)
	{
		if (Mesh->GetNumLODs() > 1) return false;
		return AuditRules::bNaniteInverted ? Mesh->NaniteSettings.bEnabled
		                                   : !Mesh->NaniteSettings.bEnabled;
	}
}

FRPDAuditRule AuditRules::Rule_NoLODs = {
	TEXT("NoLODs"),
	TEXT("Static mesh has no LOD chain — renders at full detail at all distances."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 6,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			// Defer to the Critical rule when it owns this mesh (one severity per mesh),
			// and skip meshes too small to benefit from a LOD chain.
			if (Mesh->GetNumLODs() <= 1 && !IsCriticalNoLODNanite(Mesh))
			{
				const int32 VertCount = Mesh->GetNumVertices(0);
				if (VertCount >= NoLODsMinVerts())
				{
					OutIssue.Detail = FString::Printf(TEXT("Static mesh has %d LODs. %d verts in LOD 0."), Mesh->GetNumLODs(), VertCount);
					return true;
				}
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_NoLODsAndNaniteDisabled = {
	TEXT("NoLODsAndNaniteDisabled"),
	TEXT("Static mesh has no LODs and suboptimal Nanite setting (invertible via bNaniteInverted)."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Critical, 9,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			// Same vertex gate as NoLODs — a tiny mesh with no LODs isn't critical.
			if (IsCriticalNoLODNanite(Mesh) && Mesh->GetNumVertices(0) >= NoLODsMinVerts())
			{
				const int32 VertCount = Mesh->GetNumVertices(0);
				OutIssue.Detail = AuditRules::bNaniteInverted
					? FString::Printf(TEXT("No LODs and Nanite enabled. %d verts — Nanite not expected for this project."), VertCount)
					: FString::Printf(TEXT("No LODs and Nanite disabled. %d verts — full detail always rendered."), VertCount);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_HighVertexCount = {
	TEXT("HighVertexCount"),
	TEXT("Static mesh has a high vertex count — may benefit from LODs or detail reduction."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 5,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			int32 Verts = Mesh->GetNumVertices(0);
			int32 VertWarn = RuleConfig.FindRef(TEXT("HighVertexCount_Warning"), 5000);
			int32 VertCrit = RuleConfig.FindRef(TEXT("HighVertexCount_Critical"), 10000);
			if (Verts > VertCrit)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d vertices — high polygon count. Consider LODs or mesh reduction."), Verts);
				return true;
			}
			if (Verts >= VertWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d vertices — moderately high. Verify LOD chain covers reduction."), Verts);
				OutIssue.SeverityScore = 3;
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_HighMaterialCount = {
	TEXT("HighMaterialCount"),
	TEXT("Static mesh uses many material slots — draw call overhead."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			int32 MatCount = Mesh->GetStaticMaterials().Num();
			int32 MatCountWarn = RuleConfig.FindRef(TEXT("HighMaterialCount_Warning"), 5);
			int32 MatCountCrit = RuleConfig.FindRef(TEXT("HighMaterialCount_Critical"), 8);
			if (MatCount > MatCountCrit)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d material slots — high draw call count."), MatCount);
				return true;
			}
			if (MatCount >= MatCountWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d material slots — consider combining."), MatCount);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_MissingCollision = {
	TEXT("MissingCollision"),
	TEXT("Static mesh has no collision primitive — physics will use a default box."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 7,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			if (Mesh->GetBodySetup() == nullptr)
			{
				OutIssue.Detail = TEXT("No collision body setup found.");
				return true;
			}
			const UBodySetup* BodySetup = Mesh->GetBodySetup();
			if (BodySetup->AggGeom.GetElementCount() == 0)
			{
				OutIssue.Detail = TEXT("No simple collision primitives (boxes/spheres/capsules).");
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_HighUVChannelCount = {
	TEXT("HighUVChannelCount"),
	TEXT("Static mesh uses many UV channels — extra channels add vertex shader overhead."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			int32 UVCount = Mesh->GetNumTexCoords(0);
			int32 UVWarn = RuleConfig.FindRef(TEXT("HighUVChannel_Warning"), 3);
			if (UVCount > UVWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d UV channels."), UVCount);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_MissingStaticMeshMaterial = {
	TEXT("MissingStaticMeshMaterial"),
	TEXT("Static mesh has empty material slot — will render with missing material."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Warning, 7,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			int32 MissingCount = 0;
			for (const FStaticMaterial& Mat : Mesh->GetStaticMaterials())
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

FRPDAuditRule AuditRules::Rule_SameMaterialMultipleSlots = {
	TEXT("SameMaterialMultipleSlots"),
	TEXT("Same material assigned to multiple slots — possible slot consolidation opportunity."),
	UStaticMesh::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
			TSet<UMaterialInterface*> UniqueMats;
			for (const FStaticMaterial& Mat : Materials)
			{
				if (Mat.MaterialInterface)
					UniqueMats.Add(Mat.MaterialInterface);
			}
			if (UniqueMats.Num() < Materials.Num() && Materials.Num() > 1)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d slots, %d unique materials."), Materials.Num(), UniqueMats.Num());
				return true;
			}
		}
		return false;
	}
};
