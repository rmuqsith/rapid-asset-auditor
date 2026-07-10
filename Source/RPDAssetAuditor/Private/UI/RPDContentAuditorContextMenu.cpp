// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "UI/RPDContentAuditorContextMenu.h"
#include "ContentBrowserModule.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"

#include "UI/RPDAuditWindow.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FRPDContentAuditorContextMenu"

TSharedRef<FExtender> FRPDContentAuditorContextMenu::OnExtendPathViewMenu(const TArray<FString>& SelectedPaths)
{
	TSharedRef<FExtender> MenuExtender(new FExtender());

	if (SelectedPaths.Num() > 0)
	{
		MenuExtender->AddMenuExtension(
			FName("Delete"),
			EExtensionHook::After,
			TSharedPtr<FUICommandList>(),
			FMenuExtensionDelegate::CreateLambda([SelectedPaths](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
					FText::FromString(TEXT("Audit Selected Folder(s)")),
					FText::FromString(TEXT("Run Content Auditor on the selected folders")),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Graph.AnimationFastPathIndicator"),
					FExecuteAction::CreateStatic(&FRPDContentAuditorContextMenu::OnAuditFolders, SelectedPaths)
				);

				MenuBuilder.AddMenuEntry(
					FText::FromString(TEXT("List Assets in Selected Folder(s)")),
					FText::FromString(TEXT("List all assets in the selected folders")),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Graph.AnimationFastPathIndicator"),
					FExecuteAction::CreateStatic(&FRPDContentAuditorContextMenu::OnListAssetFolders, SelectedPaths)
				);
			})
		);
	}

	return MenuExtender;
}

void FRPDContentAuditorContextMenu::OnAuditFolders(TArray<FString> SelectedPaths)
{
	// Invoke Content Auditor tab
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName("RPDAssetAuditor"));
	if (Tab.IsValid())
	{
		TSharedRef<SWidget> TabContent = Tab->GetContent();
		// Try to set folder path and trigger audit on the SRPDAuditWindow widget
		TSharedRef<SRPDAuditWindow> AuditWindow = StaticCastSharedRef<SRPDAuditWindow>(TabContent);
		AuditWindow->SetFolderPathsAndRun(SelectedPaths, true);
	}
}

void FRPDContentAuditorContextMenu::OnListAssetFolders(TArray<FString> SelectedPaths)
{
	// Invoke Content Auditor tab
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName("RPDAssetAuditor"));
	if (Tab.IsValid())
	{
		TSharedRef<SWidget> TabContent = Tab->GetContent();
		TSharedRef<SRPDAuditWindow> AuditWindow = StaticCastSharedRef<SRPDAuditWindow>(TabContent);
		AuditWindow->SetFolderPathsAndRun(SelectedPaths, false);
	}
}

#undef LOCTEXT_NAMESPACE
