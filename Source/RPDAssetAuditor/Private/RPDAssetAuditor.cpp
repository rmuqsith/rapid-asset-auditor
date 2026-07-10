// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAssetAuditor.h"
#include "ContentBrowserModule.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Application/SlateApplication.h"

#include "UI/RPDAuditWindow.h"
#include "UI/RPDContentAuditorContextMenu.h"

#define LOCTEXT_NAMESPACE "FRPDAssetAuditorModule"

// Plugin metadata for the About dialog
static const FText AboutTitle = LOCTEXT("AboutTitle", "About");
static const FText AboutText = LOCTEXT("AboutText",
	"Rapid Asset Auditor v1.1.0\n"
	"Author: Rafid Muqsith\n"
	"License: MIT\n"
	"\n"
	"Scan assets for naming, references, and quality issues");

void FRPDAssetAuditorModule::StartupModule()
{
	RegisterTab();
	RegisterMenuEntry();
	RegisterContextMenu();
}

void FRPDAssetAuditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FName("RPDAssetAuditor"));
	UnregisterMenuEntry();
	UnregisterContextMenu();
}

// ═══════════════════════════════════════════════════════════════════
// Tab
// ═══════════════════════════════════════════════════════════════════

void FRPDAssetAuditorModule::RegisterTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName("RPDAssetAuditor"),
		FOnSpawnTab::CreateRaw(this, &FRPDAssetAuditorModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Rapid Asset Auditor"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Scan assets for naming, references, and quality issues"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Graph.AnimationFastPathIndicator"));
}

TSharedRef<SDockTab> FRPDAssetAuditorModule::OnSpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabTitle", "Rapid Asset Auditor"))
		[
			SNew(SRPDAuditWindow)
		];
}

// ═══════════════════════════════════════════════════════════════════
// Menu Bar: RPD > Rapid Asset Auditor
// ═══════════════════════════════════════════════════════════════════

void FRPDAssetAuditorModule::RegisterMenuEntry()
{
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	TSharedRef<FExtender> Extender = MakeShareable(new FExtender());
	Extender->AddMenuBarExtension(
		TEXT("Help"),
		EExtensionHook::Before,
		nullptr,
		FMenuBarExtensionDelegate::CreateRaw(this, &FRPDAssetAuditorModule::AddMenuBarEntry)
	);
	LevelEditor.GetMenuExtensibilityManager()->AddExtender(Extender);
	MenuExtender = Extender;
}

void FRPDAssetAuditorModule::AddMenuBarEntry(FMenuBarBuilder& MenuBarBuilder)
{
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("RPDMenuLabel", "RPD"),
		LOCTEXT("RPDMenuTooltip", "Open RPD tools"),
		FNewMenuDelegate::CreateRaw(this, &FRPDAssetAuditorModule::AddMenuEntry),
		"RPD"
	);
}

void FRPDAssetAuditorModule::AddMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("MenuEntryTitle", "Rapid Asset Auditor"),
		LOCTEXT("MenuEntryTooltip", "Scan assets for naming, reference, and quality issues"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Graph.AnimationFastPathIndicator"),
		FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName("RPDAssetAuditor"));
		})
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AboutEntryTitle", "About"),
		LOCTEXT("AboutEntryTooltip", "Show plugin information"),
		FSlateIcon(),
		FExecuteAction::CreateLambda([]()
		{
			TSharedRef<SWindow> AboutWindow = SNew(SWindow)
				.Title(AboutTitle)
				.ClientSize(FVector2D(380, 240))
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.HasCloseButton(true)
				.SizingRule(ESizingRule::FixedSize)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(20)
					[
						SNew(STextBlock)
						.Text(AboutText)
						.AutoWrapText(true)
					]
				];
			FSlateApplication::Get().AddWindow(AboutWindow);
		})
	);
}

void FRPDAssetAuditorModule::UnregisterMenuEntry()
{
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		LevelEditor->GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);
	}
}

// ═══════════════════════════════════════════════════════════════════
// Context Menu
// ═══════════════════════════════════════════════════════════════════

void FRPDAssetAuditorModule::RegisterContextMenu()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FContentBrowserMenuExtender_SelectedPaths>& Extenders =
		ContentBrowserModule.GetAllPathViewContextMenuExtenders();

	ContextMenuExtenderHandle = Extenders.Add(
		FContentBrowserMenuExtender_SelectedPaths::CreateStatic(
			&FRPDContentAuditorContextMenu::OnExtendPathViewMenu));
}

void FRPDAssetAuditorModule::UnregisterContextMenu()
{
	if (FContentBrowserModule* ContentBrowserModule =
		FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		TArray<FContentBrowserMenuExtender_SelectedPaths>& Extenders =
			ContentBrowserModule->GetAllPathViewContextMenuExtenders();
		if (Extenders.IsValidIndex(ContextMenuExtenderHandle))
		{
			Extenders.RemoveAt(ContextMenuExtenderHandle);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRPDAssetAuditorModule, RPDAssetAuditor)
