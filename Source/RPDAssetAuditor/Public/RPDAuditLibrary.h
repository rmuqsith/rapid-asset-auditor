// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "RPDAuditLibrary.generated.h"

USTRUCT(BlueprintType)
struct FRPDTextureAuditEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    FString AssetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    FString AssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    FIntPoint Resolution = FIntPoint(0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TEnumAsByte<TextureGroup> Group = TextureGroup::TEXTUREGROUP_World;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TEnumAsByte<TextureCompressionSettings> Compression = TextureCompressionSettings::TC_Default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    bool bHasMips = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    bool bIsPowerOfTwo = false;
};

USTRUCT(BlueprintType)
struct FRPDMaterialAuditEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    FString AssetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    FString AssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TEnumAsByte<EBlendMode> BlendMode = EBlendMode::BLEND_Opaque;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TEnumAsByte<EMaterialShadingModel> ShadingModel = EMaterialShadingModel::MSM_DefaultLit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    bool bIsTwoSided = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TArray<FString> RelatedInstances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    TArray<FRPDTextureAuditEntry> UsedTextures;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audit")
    bool bIsDuplicate = false;
};

UCLASS()
class RPDASSETAUDITOR_API URPDAuditLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "RPD|Audit")
    static TArray<FRPDMaterialAuditEntry> AuditMaterialsInFolder(FString FolderPath = "/Game", bool bRecursive = true);

    UFUNCTION(BlueprintCallable, Category = "RPD|Audit")
    static TArray<FRPDTextureAuditEntry> AuditTexturesInFolder(FString FolderPath = "/Game", bool bRecursive = true);

    UFUNCTION(BlueprintCallable, Category = "RPD|Audit")
    static void FindDuplicateMaterials(UPARAM(ref) TArray<FRPDMaterialAuditEntry>& MaterialEntries);

    UFUNCTION(BlueprintCallable, Category = "RPD|Audit")
    static TArray<FRPDTextureAuditEntry> GetTexturesUsedByMaterial(UMaterialInterface* Material);
};
