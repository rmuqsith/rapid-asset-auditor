// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

/**
 * Context menu extender for Content Browser path view.
 * Adds "Audit Selected Folder(s)" and "List Assets in Selected Folder(s)"
 * when right-clicking folders in the Content Browser.
 */
class FRPDContentAuditorContextMenu
{
public:
	static TSharedRef<FExtender> OnExtendPathViewMenu(const TArray<FString>& SelectedPaths);
	static void OnAuditFolders(TArray<FString> SelectedPaths);
	static void OnListAssetFolders(TArray<FString> SelectedPaths);
};
