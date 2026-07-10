// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "RPDAuditResult.h"
#include "RPDAssetAuditManager.h"

class SScrollBox;
class SEditableTextBox;
class SEditableTextBox;
class SComboButton;


/** Column IDs for sortable header */
enum class ERPDAuditSortColumn : uint8
{
	Severity,
	Rule,
	Asset,
	Path,
	Type,
	Detail,
	Children,
	Parent
};

/**
 * Main RPD Asset Auditor Slate window.
 */
class SRPDAuditWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPDAuditWindow) {}
	SLATE_END_ARGS()

	~SRPDAuditWindow();

	void Construct(const FArguments& Args);

	void SetFolderPathsAndRun(const TArray<FString>& FolderPaths, bool bRunAudit);

	/** Navigate to a single asset by path */
	void GoToAsset(const FString& AssetPath);

	/** Navigate to a single asset from an issue */
	void GoToAsset(TSharedPtr<FRPDAuditIssue> InIssue);

	/** Navigate to multiple assets in the Content Browser */
	void GoToAssets(const TArray<FString>& AssetPaths);

	/** Called when user chooses to ignore an asset from context menu */
	void OnAssetIgnored(const FString& AssetPath);

	/** Called when user copies a row */
	void OnCopyRow(TSharedPtr<FRPDAuditIssue> InIssue);

	/** Navigate to a material instance's parent (master) asset */
	void GoToParentAsset(TSharedPtr<FRPDAuditIssue> InIssue);

	/** Select all child instances of a master material */
	void GoToChildAssets(TSharedPtr<FRPDAuditIssue> InIssue);

	/** Select all currently highlighted rows' assets in the Content Browser */
	void GoToSelectedAssets();

	/** Build the right-click context menu for the results list */
	TSharedPtr<SWidget> OnResultsContextMenu();

	/** Handle keyboard input */
	FReply OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);


private:
	void OnRunAudit();
	void OnListAssets();
	void OnBrowseFolder();
	void OnAuditComplete(const FRPDAuditResult& Result);
	bool ShouldIncludeClass(FName ClassName) const;
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildFilterPanel();
	TSharedRef<SWidget> BuildResultsPanel();
	TSharedRef<SWidget> BuildStatsPanel();
	void OnExportToCSV();
	void OnExportToHTML();
	void OnClearResults();
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRPDAuditIssue> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void SortIssues();
	void RefreshResultsList();
	void OnSortSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FName GetSortColumnFromComboSelection() const;
	TSharedRef<SHeaderRow> BuildResultHeaderRow();
	void OnSortModeChanged(EColumnSortPriority::Type SortPriority, const FName& ColumnId, EColumnSortMode::Type InSortMode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void ApplyNamingConventions();
	void UpdateNamingConventionUI();

	/** Build a threshold spinbox widget for a config key */

	/** Build a rule toggle checkbox widget */


	TSharedPtr<FRPDAssetAuditManager> AuditManager;
	FRPDAuditResult LastResult;
	TArray<TSharedPtr<FRPDAuditIssue>> AllIssues;
	TArray<TSharedPtr<FRPDAuditIssue>> FilteredIssues;
	FString SearchFilter;
	TSharedPtr<SListView<TSharedPtr<FRPDAuditIssue>>> ResultListView;

	/** Filter displayed issues based on search text */
	void ApplyFilter();

	/** Called when search text changes */
	void OnSearchTextChanged(const FText& InText);
	TSharedPtr<STextBlock> HealthScoreText;
	TSharedPtr<STextBlock> SummaryText;
	TSharedPtr<SEditableTextBox> FolderPathBox;


	bool bIncludeStaticMeshes = true;
	bool bIncludeSkeletalMeshes = true;
	bool bIncludeTextures = true;
	bool bIncludeSounds = true;
	bool bIncludeAnimations = true;
	bool bIncludeMaterials = true;
	bool bIncludeMaterialInstances = true;
	bool bIsAuditing = false;
	bool bFilterInfo = true;
	bool bFilterWarning = true;
	bool bFilterCritical = true;
	bool bFilterError = true;
	bool bSelectAll = true;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> SortComboBox;
	TArray<TSharedPtr<FString>> SortOptions;
	ERPDAuditSortColumn CurrentSortColumn = ERPDAuditSortColumn::Severity;
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Descending;
};
