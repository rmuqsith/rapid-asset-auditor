// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

TArray<FRPDMaterialAuditEntry> URPDAuditLibrary::AuditMaterialsInFolder(FString FolderPath, bool bRecursive)
{
    TArray<FRPDMaterialAuditEntry> Results;
    
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(*FolderPath);
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = bRecursive;

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    for (const FAssetData& AssetData : AssetList)
    {
        UMaterialInterface* Material = Cast<UMaterialInterface>(AssetData.GetAsset());
        if (!Material) continue;

        FRPDMaterialAuditEntry Entry;
        Entry.AssetName = AssetData.AssetName.ToString();
        Entry.AssetPath = AssetData.PackagePath.ToString();
        Entry.BlendMode = Material->GetBlendMode();
        
        if (UMaterial* BaseMaterial = Material->GetBaseMaterial())
        {
            Entry.bIsTwoSided = BaseMaterial->IsTwoSided();
        }

        Entry.UsedTextures = GetTexturesUsedByMaterial(Material);
        
        Results.Add(Entry);
    }

    return Results;
}

TArray<FRPDTextureAuditEntry> URPDAuditLibrary::AuditTexturesInFolder(FString FolderPath, bool bRecursive)
{
    TArray<FRPDTextureAuditEntry> Results;

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(*FolderPath);
    Filter.ClassPaths.Add(UTexture::StaticClass()->GetClassPathName());
    Filter.bRecursivePaths = bRecursive;

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    for (const FAssetData& AssetData : AssetList)
    {
        UTexture* Texture = Cast<UTexture>(AssetData.GetAsset());
        if (!Texture) continue;

        FRPDTextureAuditEntry Entry;
        Entry.AssetName = AssetData.AssetName.ToString();
        Entry.AssetPath = AssetData.PackagePath.ToString();
        Entry.Group = Texture->LODGroup;
        Entry.Compression = Texture->CompressionSettings;

        if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
        {
            Entry.Resolution = FIntPoint(Texture2D->GetSizeX(), Texture2D->GetSizeY());
            Entry.bIsPowerOfTwo = FMath::IsPowerOfTwo(Entry.Resolution.X) && FMath::IsPowerOfTwo(Entry.Resolution.Y);
        }

        Results.Add(Entry);
    }

    return Results;
}

void URPDAuditLibrary::FindDuplicateMaterials(TArray<FRPDMaterialAuditEntry>& MaterialEntries)
{
    TMap<FString, TArray<int32>> NameToIndexMap;

    for (int32 i = 0; i < MaterialEntries.Num(); ++i)
    {
        NameToIndexMap.FindOrAdd(MaterialEntries[i].AssetName).Add(i);
    }

    for (auto& KVP : NameToIndexMap)
    {
        if (KVP.Value.Num() > 1)
        {
            for (int32 Index : KVP.Value)
            {
                MaterialEntries[Index].bIsDuplicate = true;
            }
        }
    }
}

TArray<FRPDTextureAuditEntry> URPDAuditLibrary::GetTexturesUsedByMaterial(UMaterialInterface* Material)
{
    TArray<FRPDTextureAuditEntry> Results;
    if (!Material) return Results;

    TArray<UTexture*> UsedTextures;
    Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);

    for (UTexture* Texture : UsedTextures)
    {
        if (!Texture) continue;

        FRPDTextureAuditEntry Entry;
        Entry.AssetName = Texture->GetName();
        Entry.AssetPath = Texture->GetPathName();
        Entry.Group = Texture->LODGroup;
        Entry.Compression = Texture->CompressionSettings;

        if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
        {
            Entry.Resolution = FIntPoint(Texture2D->GetSizeX(), Texture2D->GetSizeY());
            Entry.bIsPowerOfTwo = FMath::IsPowerOfTwo(Entry.Resolution.X) && FMath::IsPowerOfTwo(Entry.Resolution.Y);
        }

        Results.Add(Entry);
    }

    return Results;
}
