// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

/**
 * Niagara audit rules.
 *
 * These load the NiagaraSystem asset to walk emitter handles and renderer
 * properties. Niagara is a first-class plugin in UE5 so the dependency is
 * safe for any UE5.5+ project.
 */

static FName NiagaraSystemClassName = TEXT("NiagaraSystem");

// ────────────────────────────────────────────────────────────────────────────
// Niagara Rule 1 — High Emitter / Renderer Count
// ────────────────────────────────────────────────────────────────────────────
FRPDAuditRule AuditRules::Rule_NiagaraHighRendererCount = {
	TEXT("NiagaraHighRendererCount"),
	TEXT("Niagara system has many emitters/renderers — potential GPU overhead."),
	NiagaraSystemClassName,
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (const UNiagaraSystem* System = Cast<const UNiagaraSystem>(AssetData.GetAsset()))
		{
			const int32 NumEmitters = System->GetNumEmitters();
			const int32 WarnNum = RuleConfig.FindRef(TEXT("NiagaraEmitters_Warning"), 8);
			const int32 CritNum = RuleConfig.FindRef(TEXT("NiagaraEmitters_Critical"), 15);

			if (NumEmitters > CritNum)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d emitters — very high system complexity, GPU cost per emitter."), NumEmitters);
				return true;
			}
			if (NumEmitters > WarnNum)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d emitters — moderate system complexity."), NumEmitters);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};

// ────────────────────────────────────────────────────────────────────────────
// Niagara Rule 2 — High Material Reference Count
// ────────────────────────────────────────────────────────────────────────────
FRPDAuditRule AuditRules::Rule_NiagaraHighMaterialCount = {
	TEXT("NiagaraHighMaterialCount"),
	TEXT("Niagara system references many unique materials — GPU sampler overhead."),
	NiagaraSystemClassName,
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (const UNiagaraSystem* System = Cast<const UNiagaraSystem>(AssetData.GetAsset()))
		{
			TSet<const UMaterialInterface*> UniqueMaterials;

			const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
			for (const FNiagaraEmitterHandle& Handle : Handles)
			{
				UNiagaraEmitter* Emitter = Handle.GetInstance();
				if (!Emitter)
					continue;

				const TArray<UNiagaraRendererProperties*> Renderers = Emitter->GetRenderers();
				for (const UNiagaraRendererProperties* Renderer : Renderers)
				{
					if (!Renderer)
						continue;

					// Most Niagara renderers expose their material via a UObjectProperty
					// named "Material". Walk properties via reflection.
					for (TFieldIterator<FObjectProperty> It(Renderer->GetClass()); It; ++It)
					{
						FObjectProperty* ObjProp = *It;
						if (!ObjProp)
							continue;

						if (ObjProp->PropertyClass &&
							ObjProp->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
						{
							if (const UMaterialInterface* Mat = Cast<UMaterialInterface>(ObjProp->GetObjectPropertyValue_InContainer(Renderer)))
							{
								UniqueMaterials.Add(Mat);
							}
						}
					}
				}
			}

			const int32 MatCount = UniqueMaterials.Num();
			const int32 WarnCount = RuleConfig.FindRef(TEXT("NiagaraMaterials_Warning"), 4);
			const int32 CritCount = RuleConfig.FindRef(TEXT("NiagaraMaterials_Critical"), 8);

			if (MatCount > CritCount)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d unique materials — high GPU cost, consider consolidating."), MatCount);
				return true;
			}
			if (MatCount > WarnCount)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d unique materials — verify they're all needed."), MatCount);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};

// ────────────────────────────────────────────────────────────────────────────
// Niagara Rule 3 — High Mesh Reference Count
// ────────────────────────────────────────────────────────────────────────────
FRPDAuditRule AuditRules::Rule_NiagaraHighMeshCount = {
	TEXT("NiagaraHighMeshCount"),
	TEXT("Niagara system references many mesh assets — draw-call overhead."),
	NiagaraSystemClassName,
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (const UNiagaraSystem* System = Cast<const UNiagaraSystem>(AssetData.GetAsset()))
		{
			TSet<const UObject*> UniqueMeshes;

			const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
			for (const FNiagaraEmitterHandle& Handle : Handles)
			{
				UNiagaraEmitter* Emitter = Handle.GetInstance();
				if (!Emitter)
					continue;

				const TArray<UNiagaraRendererProperties*> Renderers = Emitter->GetRenderers();
				for (const UNiagaraRendererProperties* Renderer : Renderers)
				{
					if (!Renderer)
						continue;

					// The mesh renderer is the only renderer that references meshes, and
					// they live in a TArray<FNiagaraMeshRendererMeshProperties> (nested
					// struct in an array), invisible to flat FObjectProperty reflection.
					if (const UNiagaraMeshRendererProperties* MeshRenderer = Cast<const UNiagaraMeshRendererProperties>(Renderer))
					{
						for (const FNiagaraMeshRendererMeshProperties& MeshProps : MeshRenderer->Meshes)
						{
							if (MeshProps.Mesh)
							{
								UniqueMeshes.Add(MeshProps.Mesh);
							}
						}
					}
				}
			}

			const int32 MeshCount = UniqueMeshes.Num();
			const int32 WarnCount = RuleConfig.FindRef(TEXT("NiagaraMeshes_Warning"), 3);
			const int32 CritCount = RuleConfig.FindRef(TEXT("NiagaraMeshes_Critical"), 6);

			if (MeshCount > CritCount)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d mesh references — high draw-call cost."), MeshCount);
				return true;
			}
			if (MeshCount > WarnCount)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d mesh references — moderate complexity."), MeshCount);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};
