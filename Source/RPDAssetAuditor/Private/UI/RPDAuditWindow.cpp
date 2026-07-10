// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "UI/RPDAuditWindow.h"
#include "UI/RPDAuditResultRow.h"
#include "RPDAuditSettingsWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlateOptMacros.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Images/SThrobber.h"
#include "Editor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWave.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"

#include "Styling/AppStyle.h"
#include "Internationalization/Text.h"
#include "RPDAuditRules.h"



#define LOCTEXT_NAMESPACE "SRPDAuditWindow"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRPDAuditWindow::Construct(const FArguments& Args)
{
	AuditManager = MakeShareable(new FRPDAssetAuditManager());

	// Connect audit completion callback
	AuditManager->OnAuditComplete().AddRaw(this, &SRPDAuditWindow::OnAuditComplete);

	// Init rule config (load from file or defaults)
	AuditRules::ResetRuleConfig();
	AuditRules::LoadConfig();

	// Default naming conventions (UE standard)
	ApplyNamingConventions();

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── Toolbar ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4)
		[
			BuildToolbar()
		]

		// ── Main content split: Filter panel (left) + Results (right) ──
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4, 0)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// Left: Filter + Stats + Naming
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(0, 0, 4, 0)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						BuildFilterPanel()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						BuildStatsPanel()
					]
				]
			]

			// Right: Results table
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				BuildResultsPanel()
			]
		]
	];
}

SRPDAuditWindow::~SRPDAuditWindow()
{
	if (AuditManager.IsValid())
	{
		AuditManager->OnAuditComplete().RemoveAll(this);
	}
}

// ═══════════════════════════════════════════════════════════════════
// Toolbar
// ═══════════════════════════════════════════════════════════════════

TSharedRef<SWidget> SRPDAuditWindow::BuildToolbar()
{
	// Initialize sort options if empty
	if (SortOptions.Num() == 0)
	{
		SortOptions.Add(MakeShareable(new FString(TEXT("Severity"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Rule"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Asset"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Path"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Type"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Detail"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Children"))));
		SortOptions.Add(MakeShareable(new FString(TEXT("Parent"))));
	}

	// Default selected item
	TSharedPtr<FString> DefaultItem = SortOptions[0];

	return SNew(SHorizontalBox)

		// ── Run Audit ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text_Lambda([this]() { return bIsAuditing ? LOCTEXT("Auditing", "Auditing...") : LOCTEXT("RunAudit", "Run Audit"); })
			.ToolTipText(LOCTEXT("RunAuditTooltip", "Scan assets for issues"))
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
			.OnClicked_Lambda([this]() { OnRunAudit(); return FReply::Handled(); })
			.IsEnabled_Lambda([this]() { return !bIsAuditing; })
		]

		// ── List Assets ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ListAssets", "List Assets"))
			.ToolTipText(LOCTEXT("ListAssetsTooltip", "List every asset matching filters (no rules)"))
			.OnClicked_Lambda([this]() { OnListAssets(); return FReply::Handled(); })
			.IsEnabled_Lambda([this]() { return !bIsAuditing; })
		]

		// ── Export to CSV ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ExportCSV", "Export to CSV"))
			.ToolTipText(LOCTEXT("ExportCSVTooltip", "Save results to a CSV file"))
			.OnClicked_Lambda([this]() { OnExportToCSV(); return FReply::Handled(); })
			.IsEnabled_Lambda([this]() { return LastResult.TotalIssues > 0; })
		]

		// ── Export to HTML ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ExportHTML", "Export to HTML"))
			.ToolTipText(LOCTEXT("ExportHTMLTooltip", "Save a shareable HTML report (material debt first)"))
			.OnClicked_Lambda([this]() { OnExportToHTML(); return FReply::Handled(); })
			.IsEnabled_Lambda([this]() { return LastResult.TotalIssues > 0; })
		]

		// ── Clear ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearResults", "Clear"))
			.ToolTipText(LOCTEXT("ClearResultsTooltip", "Clear current results"))
			.OnClicked_Lambda([this]() { OnClearResults(); return FReply::Handled(); })
			.IsEnabled_Lambda([this]() { return LastResult.TotalIssues > 0; })
		]

		// ── Spacer ──
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		// ── Sort By label ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SortBy", " Sort by:"))
			.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
		]

		// ── Sort Combo Box ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(SortComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&SortOptions)
			.OnSelectionChanged(this, &SRPDAuditWindow::OnSortSelectionChanged)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
				return SNew(STextBlock).Text(FText::FromString(*Item));
			})
			.InitiallySelectedItem(DefaultItem)
			.ContentPadding(FMargin(6.0f, 2.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (SortComboBox.IsValid() && SortComboBox->GetSelectedItem().IsValid())
						return FText::FromString(*SortComboBox->GetSelectedItem());
					return LOCTEXT("Severity", "Severity");
				})
			]
		]

		// ── Ascending toggle ──
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 0)
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("SortAscendingTooltip", "Toggle ascending / descending"))
			.IsChecked_Lambda([this]() { return (CurrentSortMode == EColumnSortMode::Ascending) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				CurrentSortMode = (State == ECheckBoxState::Checked) ? EColumnSortMode::Ascending : EColumnSortMode::Descending;
				SortIssues();
				ApplyFilter();
	RefreshResultsList();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Ascending", " Asc"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
			]
		];
}

// ═══════════════════════════════════════════════════════════════════
// Filter Panel
// ═══════════════════════════════════════════════════════════════════

TSharedRef<SWidget> SRPDAuditWindow::BuildFilterPanel()
{
	auto MakeCheckbox = [this](const FText& Label, bool& bState)
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([&bState]() { return bState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([&bState](ECheckBoxState State) { bState = (State == ECheckBoxState::Checked); })
			[
				SNew(STextBlock).Text(Label)
			];
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SVerticalBox)

			// ── Folder Path ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FolderFilter", "Content Folder"))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(FolderPathBox, SEditableTextBox)
						.Text(FText::FromString(TEXT("/Game")))
						.ToolTipText(LOCTEXT("FolderPathTooltip", "Content folder to scan (e.g. /Game/Environment). Leave as /Game for entire project."))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("...")))
						.ToolTipText(LOCTEXT("BrowseFolder", "Browse for a content folder"))
						.OnClicked_Lambda([this]() { OnBrowseFolder(); return FReply::Handled(); })
					]
				]
			]

			// ── Asset Type Filters ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetFilters", "Asset Type Filters"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]

			// Select/Deselect All
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bSelectAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bSelectAll = (State == ECheckBoxState::Checked);
					bIncludeStaticMeshes = bSelectAll;
					bIncludeSkeletalMeshes = bSelectAll;
					bIncludeTextures = bSelectAll;
					bIncludeSounds = bSelectAll;
					bIncludeAnimations = bSelectAll;
					bIncludeMaterials = bSelectAll;
					bIncludeMaterialInstances = bSelectAll;
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SelectAllAssets", "Select/Deselect All"))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("StaticMeshes", "Static Meshes"), bIncludeStaticMeshes) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("SkeletalMeshes", "Skeletal Meshes"), bIncludeSkeletalMeshes) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("Textures", "Textures"), bIncludeTextures) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("Sounds", "Sounds"), bIncludeSounds) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("Animations", "Animations"), bIncludeAnimations) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("Materials", "Materials"), bIncludeMaterials) ]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[ MakeCheckbox(LOCTEXT("MaterialInstances", "Material Instances"), bIncludeMaterialInstances) ]

			// ── Severity Filter ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SeverityFilter", "Filter by Severity"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[ MakeCheckbox(LOCTEXT("FilterInfo", "Info"), bFilterInfo) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[ MakeCheckbox(LOCTEXT("FilterWarning", "Warning"), bFilterWarning) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[ MakeCheckbox(LOCTEXT("FilterCritical", "Critical / Error"), bFilterCritical) ]

			// ── Audit Settings ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AuditSettings", "Audit Settings"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 2)
			[
				SNew(SButton)
				.Text(LOCTEXT("AuditSettings", "Audit Settings..."))
				.ToolTipText(LOCTEXT("AuditSettingsTooltip", "Configure rules, thresholds, and naming conventions"))
				.OnClicked_Lambda([]() { SRPDAuditSettingsWindow::Open(); return FReply::Handled(); })
			]
		];}

// ═══════════════════════════════════════════════════════════════════
// Results Panel
// ═══════════════════════════════════════════════════════════════════

TSharedRef<SWidget> SRPDAuditWindow::BuildResultsPanel()
{
	return SNew(SVerticalBox)

		// ── Summary Bar ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 0, 4, 4)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8, 4))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 12, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (LastResult.TotalIssues == 0)
							return LOCTEXT("NoResults", "No issues found");
						return FText::Format(LOCTEXT("TotalIssues", "{0} issues"), FText::AsNumber(LastResult.TotalIssues));
					})
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]

				// Info: blue
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						int32 C = 0;
						for (auto& I : LastResult.Issues)
							if (I.Severity == ERPDAuditSeverity::Info) C++;
						return FText::Format(LOCTEXT("InfoCount", "Info: {0}"), FText::AsNumber(C));
					})
					.ColorAndOpacity(FLinearColor(0.3f, 0.6f, 1.0f))
				]

				// Warning: amber
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						int32 C = 0;
						for (auto& I : LastResult.Issues)
							if (I.Severity == ERPDAuditSeverity::Warning) C++;
						return FText::Format(LOCTEXT("WarningCount", "Warning: {0}"), FText::AsNumber(C));
					})
					.ColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.1f))
				]

				// Critical: red
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						int32 C = 0;
						for (auto& I : LastResult.Issues)
							if (I.Severity == ERPDAuditSeverity::Critical || I.Severity == ERPDAuditSeverity::Error) C++;
						return FText::Format(LOCTEXT("CriticalCount", "Critical: {0}"), FText::AsNumber(C));
					})
					.ColorAndOpacity(FLinearColor(1.0f, 0.2f, 0.2f))
				]

				// Health score
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (HealthScoreText.IsValid())
							return HealthScoreText->GetText();
						return LOCTEXT("Score", "Score: —");
					})
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]

				// Filtered count
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (LastResult.TotalIssues > 0 && FilteredIssues.Num() != LastResult.TotalIssues)
							return FText::Format(LOCTEXT("FilteredCount", "showing {0}"), FText::AsNumber(FilteredIssues.Num()));
						return FText::GetEmpty();
					})
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]

		// ── Search Box ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 0, 4, 4)
		[
			SNew(SEditableTextBox)
			.HintText(LOCTEXT("SearchHint", "Filter assets by name..."))
			.OnTextChanged_Raw(this, &SRPDAuditWindow::OnSearchTextChanged)
			.MinDesiredWidth(200)
		]

		// ── Result List ──
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(4)
			[
				SAssignNew(ResultListView, SListView<TSharedPtr<FRPDAuditIssue>>)
				.ListItemsSource(&FilteredIssues)
				.OnGenerateRow(this, &SRPDAuditWindow::OnGenerateRow)
				.HeaderRow(BuildResultHeaderRow())
				.SelectionMode(ESelectionMode::Multi)
				.OnKeyDownHandler_Raw(this, &SRPDAuditWindow::OnKeyDownHandler)
				.OnContextMenuOpening_Raw(this, &SRPDAuditWindow::OnResultsContextMenu)
			]
		];
}

TSharedRef<SHeaderRow> SRPDAuditWindow::BuildResultHeaderRow()
{
	auto AddColumn = [this](const FName& ColumnId, const FText& Label,
		float Width)
	{
		FOnSortModeChanged OnSort = FOnSortModeChanged::CreateRaw(this, &SRPDAuditWindow::OnSortModeChanged);
		return SHeaderRow::Column(ColumnId)
			.DefaultLabel(Label)
			.ManualWidth(Width)
			.HAlignCell(HAlign_Center)
			.SortMode_Raw(this, &SRPDAuditWindow::GetColumnSortMode, ColumnId)
			.OnSort(OnSort)
			.ShouldGenerateWidget(true);
	};

	auto AddFillColumn = [this](const FName& ColumnId, const FText& Label, float FillWidth)
	{
		FOnSortModeChanged OnSort = FOnSortModeChanged::CreateRaw(this, &SRPDAuditWindow::OnSortModeChanged);
		return SHeaderRow::Column(ColumnId)
			.DefaultLabel(Label)
			.FillWidth(FillWidth)
			.SortMode_Raw(this, &SRPDAuditWindow::GetColumnSortMode, ColumnId)
			.OnSort(OnSort)
			.ShouldGenerateWidget(true);
	};

	return SNew(SHeaderRow)

		+ AddColumn(TEXT("Severity"), LOCTEXT("SeverityColumn", ""), 36)
		+ AddColumn(TEXT("Rule"), LOCTEXT("RuleColumn", "Rule"), 140)
		+ AddFillColumn(TEXT("Asset"), LOCTEXT("AssetColumn", "Asset"), 0.20f)
		+ AddFillColumn(TEXT("Path"), LOCTEXT("PathColumn", "Path"), 0.28f)
		+ AddFillColumn(TEXT("Parent"), LOCTEXT("ParentColumn", "Parent"), 0.13f)
		+ AddColumn(TEXT("Children"), LOCTEXT("ChildrenColumn", "Children"), 70)
		+ AddFillColumn(TEXT("Type"), LOCTEXT("TypeColumn", "Type"), 0.13f)
		+ AddFillColumn(TEXT("Detail"), LOCTEXT("DetailColumn", "Detail"), 0.45f)
		+ SHeaderRow::Column(TEXT("Ignore"))
			.FixedWidth(32)
			.HAlignHeader(HAlign_Center)
			.DefaultLabel(FText::GetEmpty())
			.ShouldGenerateWidget(true);
}

TSharedRef<ITableRow> SRPDAuditWindow::OnGenerateRow(TSharedPtr<FRPDAuditIssue> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRPDAuditResultRow, OwnerTable)
		.Issue(InItem)
		.OnDoubleClicked(FOnRowDoubleClicked::CreateRaw(this, &SRPDAuditWindow::GoToAsset))
		.OnAssetIgnored(FOnAssetIgnored::CreateRaw(this, &SRPDAuditWindow::OnAssetIgnored))
		.OnCopy(FOnRowCopy::CreateRaw(this, &SRPDAuditWindow::OnCopyRow))
		.OnGoToParent(FOnGoToParent::CreateRaw(this, &SRPDAuditWindow::GoToParentAsset))
		.OnGoToChildren(FOnGoToChildren::CreateRaw(this, &SRPDAuditWindow::GoToChildAssets))
		.OnGoToSelected(FOnGoToSelected::CreateRaw(this, &SRPDAuditWindow::GoToSelectedAssets));
}

// ═══════════════════════════════════════════════════════════════════
// Stats Panel
// ═══════════════════════════════════════════════════════════════════

TSharedRef<SWidget> SRPDAuditWindow::BuildStatsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HealthScoreLabel", "Health Score"))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SAssignNew(HealthScoreText, STextBlock)
				.Text(LOCTEXT("NoScore", "—"))
				.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 1.0f, 0.3f)))
			]

			// ── Loading indicator ──
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SThrobber)
				.Visibility_Lambda([this]() { return bIsAuditing ? EVisibility::Visible : EVisibility::Collapsed; })
				.PieceImage(FAppStyle::Get().GetBrush("Throbber.CircleChunk"))
				.NumPieces(5)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(SummaryText, STextBlock)
				.Text(LOCTEXT("NoResults", "No audit results yet."))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.AutoWrapText(true)
			]
		];
}

// ═══════════════════════════════════════════════════════════════════
// Audit Execution
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::OnBrowseFolder()
{
	// Use SPathPicker or a simple text input for folder selection.
	// For simplicity, we use a desktop folder dialog via content browser path.
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform) return;

	void* ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid()
		? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	FString OutFolder;
	if (Platform->OpenDirectoryDialog(ParentWindowHandle, TEXT("Select Content Folder"), TEXT(""), OutFolder))
	{
		// Convert filesystem path to content browser path (/Game/...)
		FString ContentPath = OutFolder;
		// Find the Content directory and extract the relative path
		int32 ContentIdx = ContentPath.Find(TEXT("/Content"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (ContentIdx != INDEX_NONE)
		{
			ContentPath = ContentPath.RightChop(ContentIdx + 8); // skip "/Content"
			ContentPath.ReplaceInline(TEXT("\\"), TEXT("/"));
			ContentPath = TEXT("/Game") + ContentPath;
			
			if (FolderPathBox.IsValid())
			{
				FolderPathBox->SetText(FText::FromString(ContentPath));
			}
		}
	}
}

void SRPDAuditWindow::OnRunAudit()
{
	if (bIsAuditing || !AuditManager.IsValid())
		return;

	bIsAuditing = true;

	// The progress/cancel dialog is owned by the manager's per-asset scan loop.

	// Set Nanite inversion flag
	AuditRules::SaveConfig();

	// Apply naming conventions from UI
	ApplyNamingConventions();

	// Get folder path from UI
	FString RootPath = TEXT("/Game");
	if (FolderPathBox.IsValid())
	{
		FString FolderText = FolderPathBox->GetText().ToString();
		FolderText.TrimStartAndEndInline();
		if (!FolderText.IsEmpty())
		{
			RootPath = FolderText;
		}
	}

	// Build filter with selected classes
	TArray<FTopLevelAssetPath> ClassPaths;
	if (bIncludeStaticMeshes)
		ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	if (bIncludeSkeletalMeshes)
		ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	if (bIncludeTextures)
		ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
	if (bIncludeSounds)
		ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
	if (bIncludeAnimations)
		ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	if (bIncludeMaterials)
	{
		ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	}
	if (bIncludeMaterialInstances)
	{
		ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
	}
	// Run audit for specified folder
	FRPDAuditResult Result = AuditManager->RunAuditForClasses(ClassPaths, RootPath, true);
	OnAuditComplete(Result);
}

void SRPDAuditWindow::OnListAssets()
{
	if (bIsAuditing || !AuditManager.IsValid())
		return;

	bIsAuditing = true;

	// Set Nanite inversion flag
	AuditRules::SaveConfig();

	// Get folder path from UI
	FString RootPath = TEXT("/Game");
	if (FolderPathBox.IsValid())
	{
		FString FolderText = FolderPathBox->GetText().ToString();
		FolderText.TrimStartAndEndInline();
		if (!FolderText.IsEmpty())
		{
			RootPath = FolderText;
		}
	}

	// Build filter with selected classes
	TArray<FTopLevelAssetPath> ClassPaths;
	if (bIncludeStaticMeshes)
		ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	if (bIncludeSkeletalMeshes)
		ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	if (bIncludeTextures)
		ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
	if (bIncludeSounds)
		ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
	if (bIncludeAnimations)
		ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	if (bIncludeMaterials)
		ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	if (bIncludeMaterialInstances)
		ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
	// List all assets (no rules applied)
	FRPDAuditResult Result = AuditManager->ListAssets(ClassPaths, RootPath, true);
	OnAuditComplete(Result);
}

void SRPDAuditWindow::OnAuditComplete(const FRPDAuditResult& Result)
{
	LastResult = Result;
	AllIssues.Empty();
	FilteredIssues.Empty();

	for (const FRPDAuditIssue& Issue : Result.Issues)
	{
		AllIssues.Add(MakeShareable(new FRPDAuditIssue(Issue)));
	}

	SortIssues();
	ApplyFilter();
	RefreshResultsList();

	// Update stats
	if (HealthScoreText.IsValid())
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 1;
		Options.MinimumFractionalDigits = 1;
		HealthScoreText->SetText(FText::AsNumber(Result.HealthScore, &Options));

		// Color based on score (smooth gradient)
		float Score = FMath::Clamp(Result.HealthScore, 0.0f, 100.0f);
		FLinearColor Color;
		if (Score >= 80.0f)
			Color = FLinearColor(0.3f, 1.0f, 0.3f); // Green
		else if (Score >= 50.0f)
		{
			// Smooth amber: lerp from green to yellow
			float T = (Score - 50.0f) / 30.0f;
			Color = FMath::Lerp(FLinearColor(1.0f, 0.8f, 0.2f), FLinearColor(0.3f, 1.0f, 0.3f), T);
		}
		else
		{
			// Smooth red: lerp from red to amber
			float T = Score / 50.0f;
			Color = FMath::Lerp(FLinearColor(1.0f, 0.2f, 0.2f), FLinearColor(1.0f, 0.8f, 0.2f), T);
		}
		HealthScoreText->SetColorAndOpacity(FSlateColor(Color));
	}

	if (SummaryText.IsValid())
	{
		FText Summary;
		if (Result.ScannedAssetCount > 0)
		{
			TSet<FString> ProblemAssets;
			for (const FRPDAuditIssue& Issue : Result.Issues)
			{
				if (Issue.Severity != ERPDAuditSeverity::Info)
					ProblemAssets.Add(Issue.AssetPath);
			}
			int32 ProblemCount = ProblemAssets.Num();
			int32 CleanAssets = Result.ScannedAssetCount - ProblemCount;
			int32 NonInfoIssues = Result.TotalIssues - Result.InfoCount;
			Summary = FText::Format(
				LOCTEXT("SummaryWithHealth", "{0} of {1} assets clean — {2} asset(s) with issues, {3} total finding(s)"),
				FText::AsNumber(CleanAssets),
				FText::AsNumber(Result.ScannedAssetCount),
				FText::AsNumber(ProblemCount),
				FText::AsNumber(Result.TotalIssues)
			);
		}
		else
		{
			Summary = FText::Format(
				LOCTEXT("SummaryBasic", "{0} issues: {1} critical, {2} warning, {3} info"),
				FText::AsNumber(Result.TotalIssues),
				FText::AsNumber(Result.CriticalCount),
				FText::AsNumber(Result.WarningCount),
				FText::AsNumber(Result.InfoCount)
			);
		}
		if (Result.bCancelled)
		{
			Summary = FText::Format(LOCTEXT("AuditSummaryCancelled", "{0}  —  scan cancelled, partial results"), Summary);
		}
		SummaryText->SetText(Summary);
	}

	bIsAuditing = false;
}

bool SRPDAuditWindow::ShouldIncludeClass(FName ClassName) const
{
	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Export / Clear
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::OnExportToCSV()
{
	if (LastResult.TotalIssues == 0)
		return;

	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform)
		return;

	void* ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid()
		? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	if (Platform->SaveFileDialog(
		ParentWindowHandle,
		TEXT("Export Audit Results"),
		TEXT(""),
		TEXT("AuditResults.csv"),
		TEXT("CSV Files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles))
	{
		if (OutFiles.Num() > 0)
		{
			// RFC 4180: quote a field only if it holds a comma, quote, or newline;
			// double any inner quotes. Detail regularly contains commas, so without
			// this the row splits into extra columns and downstream parsers misalign.
			auto CsvField = [](const FString& In) -> FString
			{
				if (!In.Contains(TEXT(",")) && !In.Contains(TEXT("\"")) &&
					!In.Contains(TEXT("\n")) && !In.Contains(TEXT("\r")))
					return In;
				return FString::Printf(TEXT("\"%s\""), *In.Replace(TEXT("\""), TEXT("\"\"")));
			};

			FString CSVContent;
			CSVContent += TEXT("Severity,Rule,Asset,AssetPath,Type,Parent,ChildrenCount,Detail\n");
			for (const FRPDAuditIssue& Issue : LastResult.Issues)
			{
				const FString SeverityStr = StaticEnum<ERPDAuditSeverity>()->GetNameStringByValue((int64)Issue.Severity);
				// ChildCount is only meaningful for hierarchy rules (materials); blank when N/A (< 0).
				const FString ChildrenStr = Issue.ChildCount >= 0 ? FString::FromInt(Issue.ChildCount) : FString();
				CSVContent += FString::Printf(TEXT("%s,%s,%s,%s,%s,%s,%s,%s\n"),
					*CsvField(SeverityStr),
					*CsvField(Issue.RuleName),
					*CsvField(Issue.AssetName),
					*CsvField(Issue.AssetPath),
					*CsvField(Issue.AssetType),
					*CsvField(Issue.ParentName),
					*ChildrenStr,
					*CsvField(Issue.Detail));
			}
			// Force UTF-8 so the encoding is stable regardless of whether any asset
			// name/detail contains a non-ANSI char (default AutoDetect would flip to UTF-16).
			FFileHelper::SaveStringToFile(CSVContent, *OutFiles[0], FFileHelper::EEncodingOptions::ForceUTF8);
		}
	}
}

void SRPDAuditWindow::OnExportToHTML()
{
	if (LastResult.TotalIssues == 0)
		return;

	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform)
		return;

	void* ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid()
		? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	TArray<FString> OutFiles;
	if (!Platform->SaveFileDialog(ParentWindowHandle, TEXT("Export Audit Report"), TEXT(""),
		TEXT("AuditReport.html"), TEXT("HTML Files (*.html)|*.html"), EFileDialogFlags::None, OutFiles)
		|| OutFiles.Num() == 0)
		return;

	auto Esc = [](const FString& In) -> FString
	{
		return In.Replace(TEXT("&"), TEXT("&amp;")).Replace(TEXT("<"), TEXT("&lt;"))
			.Replace(TEXT(">"), TEXT("&gt;")).Replace(TEXT("\""), TEXT("&quot;"));
	};
	auto SevColor = [](ERPDAuditSeverity S) -> const TCHAR*
	{
		if (S == ERPDAuditSeverity::Critical || S == ERPDAuditSeverity::Error) return TEXT("#e5484d");
		if (S == ERPDAuditSeverity::Warning) return TEXT("#f5a623");
		return TEXT("#5b8def");
	};
	auto SevOrder = [](ERPDAuditSeverity S) -> int32
	{
		if (S == ERPDAuditSeverity::Critical || S == ERPDAuditSeverity::Error) return 0;
		if (S == ERPDAuditSeverity::Warning) return 1;
		return 2;
	};
	// Each rule maps to one art-team-facing category; material debt leads.
	auto CatOf = [](const FString& R) -> int32
	{
		static const TSet<FString> Mat = {
			TEXT("MasterMaterialBloat"),TEXT("MaterialInstanceBloat"),TEXT("MaterialInstanceMinimalOverrides"),
			TEXT("ListAllMasterMaterials"),TEXT("HighTextureSampling"),TEXT("BlendModeMismatch"),
			TEXT("ShadingModelMismatch"),TEXT("OneOffMaterial"),TEXT("DuplicateMaterialInstance"),
			TEXT("UnusedMasterParameters"),TEXT("DirectMasterMaterialOnStaticMesh"),
			TEXT("DirectMasterMaterialOnSkeletalMesh"),TEXT("SameMaterialMultipleSlots"),
			TEXT("MissingStaticMeshMaterial"),TEXT("MissingSkeletalMeshMaterial") };
		static const TSet<FString> Mesh = {
			TEXT("NoLODs"),TEXT("NoLODsAndNaniteDisabled"),TEXT("HighVertexCount"),TEXT("SkeletalMesh_NoLODs"),
			TEXT("SkeletalMesh_HighVertexCount"),TEXT("MissingCollision"),TEXT("MissingPhysicsAsset"),
			TEXT("HighUVChannelCount"),TEXT("HighMaterialCount"),TEXT("SkeletalMesh_HighMaterialCount") };
		static const TSet<FString> Tex = {
			TEXT("NonPowerOfTwo"),TEXT("LargeTexture"),TEXT("NeverStream"),TEXT("TextureLODGroup"),
			TEXT("VirtualTextureMismatch") };
		if (Mat.Contains(R)) return 0;
		if (Mesh.Contains(R)) return 1;
		if (Tex.Contains(R)) return 2;
		if (R.StartsWith(TEXT("Naming_"))) return 3;
		return 4;
	};
	const TCHAR* CatNames[5] = { TEXT("Material Debt"), TEXT("Meshes &amp; LODs"), TEXT("Textures"),
		TEXT("Naming"), TEXT("Audio, Blueprint &amp; Misc") };

	// Bucket issues by rule and count severities.
	TMap<FString, TArray<const FRPDAuditIssue*>> ByRule;
	int32 Crit = 0, Warn = 0, Info = 0;
	for (const FRPDAuditIssue& I : LastResult.Issues)
	{
		ByRule.FindOrAdd(I.RuleName).Add(&I);
		const int32 O = SevOrder(I.Severity);
		if (O == 0) Crit++; else if (O == 1) Warn++; else Info++;
	}

	FString Body;
	for (int32 Cat = 0; Cat < 5; ++Cat)
	{
		TArray<FString> Rules;
		for (const auto& KVP : ByRule)
			if (CatOf(KVP.Key) == Cat) Rules.Add(KVP.Key);
		if (Rules.Num() == 0) continue;

		Rules.Sort([&](const FString& A, const FString& B)
		{
			const int32 SA = SevOrder(ByRule[A][0]->Severity), SB = SevOrder(ByRule[B][0]->Severity);
			return SA != SB ? SA < SB : ByRule[A].Num() > ByRule[B].Num();
		});

		int32 CatCount = 0;
		for (const FString& R : Rules) CatCount += ByRule[R].Num();

		Body += FString::Printf(TEXT("<section%s><h2>%s<span class=\"gcount\">%d</span></h2>"),
			Cat == 0 ? TEXT(" class=\"lead\"") : TEXT(""), CatNames[Cat], CatCount);

		for (const FString& R : Rules)
		{
			const TArray<const FRPDAuditIssue*>& Items = ByRule[R];
			const TCHAR* Col = SevColor(Items[0]->Severity);
			const FString SevStr = StaticEnum<ERPDAuditSeverity>()->GetNameStringByValue((int64)Items[0]->Severity);
			Body += FString::Printf(TEXT("<details class=\"rule\"><summary><span class=\"dot\" style=\"background:%s\"></span><b>%s</b><span class=\"cnt\">%d</span><span class=\"sev\" style=\"color:%s\">%s</span></summary><div class=\"rows\">"),
				Col, *Esc(R), Items.Num(), Col, *Esc(SevStr));

			int32 Shown = 0;
			for (const FRPDAuditIssue* I : Items)
			{
				if (Shown++ >= 500)
				{
					Body += FString::Printf(TEXT("<div class=\"more\">&hellip;and %d more</div>"), Items.Num() - 500);
					break;
				}
				Body += FString::Printf(TEXT("<div class=\"row\"><div class=\"asset\">%s</div><div class=\"path\">%s</div><div class=\"detail\">%s</div></div>"),
					*Esc(I->AssetName), *Esc(I->GetAssetFolder()), *Esc(I->Detail));
			}
			Body += TEXT("</div></details>");
		}
		Body += TEXT("</section>");
	}

	FString Html = TEXT("<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Asset Audit Report</title><style>");
	Html += TEXT("*{box-sizing:border-box}body{margin:0;font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif;background:#14161a;color:#dfe3e8}");
	Html += TEXT(".wrap{max-width:1100px;margin:0 auto;padding:28px 20px}h1{font-size:22px;margin:0 0 2px}.sub{color:#8a93a0;font-size:13px;margin-bottom:20px}");
	Html += TEXT(".kpis{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:26px}.kpi{flex:1;min-width:120px;background:#1c1f26;border:1px solid #262b34;border-radius:10px;padding:14px 16px}");
	Html += TEXT(".kpi .n{font-size:26px;font-weight:700}.kpi .l{color:#8a93a0;font-size:12px;text-transform:uppercase;letter-spacing:.04em}section{margin-bottom:26px}");
	Html += TEXT("h2{font-size:15px;text-transform:uppercase;letter-spacing:.05em;color:#aeb6c2;border-bottom:1px solid #262b34;padding-bottom:8px;display:flex;align-items:center}");
	Html += TEXT(".lead{background:#1a1d24;border:1px solid #2d333d;border-radius:12px;padding:16px 18px}.gcount{margin-left:auto;font-size:12px;background:#2a2f38;color:#cfd6de;border-radius:20px;padding:2px 10px}");
	Html += TEXT("details.rule{border:1px solid #262b34;border-radius:8px;margin:8px 0;background:#1a1d23;overflow:hidden}summary{cursor:pointer;padding:10px 14px;display:flex;align-items:center;gap:10px;list-style:none}summary::-webkit-details-marker{display:none}");
	Html += TEXT(".dot{width:9px;height:9px;border-radius:50%;flex:none}.cnt{background:#2a2f38;border-radius:20px;padding:1px 9px;font-size:12px;color:#cfd6de}.sev{margin-left:auto;font-size:11px;text-transform:uppercase;letter-spacing:.05em;font-weight:600}");
	Html += TEXT(".rows{border-top:1px solid #262b34}.row{display:grid;grid-template-columns:1.1fr 1.4fr 2.5fr;gap:12px;padding:8px 14px;border-bottom:1px solid #21252c;font-size:13px}.row:last-child{border-bottom:none}");
	Html += TEXT(".asset{font-weight:600;color:#eef1f5;word-break:break-word}.path{color:#7f8896;font-family:ui-monospace,monospace;font-size:12px;word-break:break-all}.detail{color:#c3cad3}.more{padding:8px 14px;color:#7f8896;font-style:italic}");
	Html += TEXT("@media(max-width:720px){.row{grid-template-columns:1fr}}");
	Html += TEXT("</style></head><body><div class=\"wrap\">");
	Html += FString::Printf(TEXT("<h1>Asset Audit Report</h1><div class=\"sub\">%s &middot; %d findings &middot; health %.0f/100</div>"),
		*Esc(LastResult.ProjectName), LastResult.TotalIssues, LastResult.HealthScore);
	Html += FString::Printf(TEXT("<div class=\"kpis\"><div class=\"kpi\"><div class=\"n\">%d</div><div class=\"l\">Total</div></div>")
		TEXT("<div class=\"kpi\"><div class=\"n\" style=\"color:#e5484d\">%d</div><div class=\"l\">Critical</div></div>")
		TEXT("<div class=\"kpi\"><div class=\"n\" style=\"color:#f5a623\">%d</div><div class=\"l\">Warning</div></div>")
		TEXT("<div class=\"kpi\"><div class=\"n\" style=\"color:#5b8def\">%d</div><div class=\"l\">Info</div></div></div>"),
		LastResult.TotalIssues, Crit, Warn, Info);
	Html += Body;
	Html += TEXT("<div class=\"sub\" style=\"margin-top:30px\">Generated by Rapid Asset Auditor.</div></div></body></html>");

	FFileHelper::SaveStringToFile(Html, *OutFiles[0], FFileHelper::EEncodingOptions::ForceUTF8);
}

void SRPDAuditWindow::OnClearResults()
{
	LastResult = FRPDAuditResult();
	AllIssues.Empty();
	FilteredIssues.Empty();
	ApplyFilter();
	RefreshResultsList();

	if (HealthScoreText.IsValid())
		HealthScoreText->SetText(LOCTEXT("NoScore", "—"));
	if (SummaryText.IsValid())
		SummaryText->SetText(LOCTEXT("NoResults", "No audit results yet."));
}

// ═══════════════════════════════════════════════════════════════════
// Result List Helpers
// ═══════════════════════════════════════════════════════════════════

static ERPDAuditSortColumn ColumnIdToSortColumn(const FName& ColumnId)
{
	if (ColumnId == TEXT("Severity")) return ERPDAuditSortColumn::Severity;
	if (ColumnId == TEXT("Rule")) return ERPDAuditSortColumn::Rule;
	if (ColumnId == TEXT("Asset")) return ERPDAuditSortColumn::Asset;
	if (ColumnId == TEXT("Path")) return ERPDAuditSortColumn::Path;
	if (ColumnId == TEXT("Type")) return ERPDAuditSortColumn::Type;
	if (ColumnId == TEXT("Children")) return ERPDAuditSortColumn::Children;
	if (ColumnId == TEXT("Parent")) return ERPDAuditSortColumn::Parent;
	return ERPDAuditSortColumn::Detail;
}

// ═══════════════════════════════════════════════════════════════════
// Sort methods
// ═══════════════════════════════════════════════════════════════════

FName SRPDAuditWindow::GetSortColumnFromComboSelection() const
{
	if (SortComboBox.IsValid() && SortComboBox->GetSelectedItem().IsValid())
	{
		return FName(**SortComboBox->GetSelectedItem());
	}
	return FName(TEXT("Severity"));
}

void SRPDAuditWindow::OnSortSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		CurrentSortColumn = ColumnIdToSortColumn(FName(**NewSelection));
		// Sync sort mode: keep existing direction, or default to Descending for new column
		CurrentSortMode = EColumnSortMode::Descending;
		SortIssues();
		ApplyFilter();
	RefreshResultsList();
	}
}

void SRPDAuditWindow::OnSortModeChanged(EColumnSortPriority::Type SortPriority, const FName& ColumnId, EColumnSortMode::Type InSortMode)
{
	CurrentSortColumn = ColumnIdToSortColumn(ColumnId);
	CurrentSortMode = InSortMode;

	// Sync dropdown to match header click
	if (SortComboBox.IsValid() && SortOptions.Num() > 0)
	{
		FString ColumnName = ColumnId.ToString();
		for (const TSharedPtr<FString>& Option : SortOptions)
		{
			if (*Option == ColumnName)
			{
				SortComboBox->SetSelectedItem(Option);
				break;
			}
		}
	}

	SortIssues();
	ApplyFilter();
	RefreshResultsList();
}

EColumnSortMode::Type SRPDAuditWindow::GetColumnSortMode(const FName ColumnId) const
{
	if (ColumnIdToSortColumn(ColumnId) == CurrentSortColumn)
	{
		return CurrentSortMode;
	}
	return EColumnSortMode::None;
}

void SRPDAuditWindow::SortIssues()
{
	// Sort using CurrentSortColumn + CurrentSortMode
	FName SortColumnName = GetSortColumnFromComboSelection();
	CurrentSortColumn = ColumnIdToSortColumn(SortColumnName);

	AllIssues.Sort([this](const TSharedPtr<FRPDAuditIssue>& A, const TSharedPtr<FRPDAuditIssue>& B)
	{
		if (!A.IsValid() || !B.IsValid()) return false;

		bool bResult = false;

		switch (CurrentSortColumn)
		{
		case ERPDAuditSortColumn::Severity:
			bResult = static_cast<int32>(A->Severity) < static_cast<int32>(B->Severity);
			break;
		case ERPDAuditSortColumn::Rule:
			bResult = A->RuleName < B->RuleName;
			break;
		case ERPDAuditSortColumn::Asset:
			bResult = A->AssetName < B->AssetName;
			break;
		case ERPDAuditSortColumn::Path:
		{
			// Group by folder; within the same folder, order by asset name.
			const FString FolderA = A->GetAssetFolder();
			const FString FolderB = B->GetAssetFolder();
			bResult = (FolderA == FolderB) ? (A->AssetName < B->AssetName) : (FolderA < FolderB);
			break;
		}
		case ERPDAuditSortColumn::Type:
			bResult = A->AssetType < B->AssetType;
			break;
		case ERPDAuditSortColumn::Detail:
			bResult = A->Detail < B->Detail;
			break;
		case ERPDAuditSortColumn::Children:
			bResult = A->ChildCount < B->ChildCount;
			break;
		case ERPDAuditSortColumn::Parent:
			bResult = A->ParentName < B->ParentName;
			break;
		default:
			bResult = A->AssetName < B->AssetName;
			break;
		}

		// Use CurrentSortMode to determine direction
		if (CurrentSortMode == EColumnSortMode::Descending)
		{
			bResult = !bResult;
		}

		return bResult;
	});
}

void SRPDAuditWindow::RefreshResultsList()
{
	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}
}

// ═══════════════════════════════════════════════════════════════════
// Naming Conventions
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::ApplyNamingConventions()
{
	if (!AuditManager.IsValid())
		return;

	TArray<FRPDNamingConvention> Conventions;

	auto AddPrefix = [&](const FString& Key, FName TargetClass)
	{
		FString Prefix = AuditRules::NamingPrefixes.FindRef(Key);
		if (!Prefix.IsEmpty())
			Conventions.Add({ Prefix, TEXT(""), TargetClass });
	};

	AddPrefix(TEXT("SM_"), UStaticMesh::StaticClass()->GetFName());
	AddPrefix(TEXT("SK_"), USkeletalMesh::StaticClass()->GetFName());
	AddPrefix(TEXT("T_"), UTexture2D::StaticClass()->GetFName());
	AddPrefix(TEXT("S_"), USoundWave::StaticClass()->GetFName());
	AddPrefix(TEXT("MI_"), UMaterialInstanceConstant::StaticClass()->GetFName());
	AddPrefix(TEXT("A_"), UAnimSequence::StaticClass()->GetFName());

	// Materials and post-process materials are both UMaterial — enforce a single
	// prefix per domain so the two aren't mixed (surface = M_, post-process = PPM_).
	auto AddMaterialPrefix = [&](const FString& Key, ERPDMaterialDomainFilter Domain)
	{
		FString Prefix = AuditRules::NamingPrefixes.FindRef(Key);
		if (!Prefix.IsEmpty())
			Conventions.Add({ Prefix, TEXT(""), UMaterial::StaticClass()->GetFName(), Domain });
	};
	AddMaterialPrefix(TEXT("M_"), ERPDMaterialDomainFilter::SurfaceOnly);
	AddMaterialPrefix(TEXT("PPM_"), ERPDMaterialDomainFilter::PostProcess);

	AuditManager->SetNamingConventions(Conventions);
}

void SRPDAuditWindow::UpdateNamingConventionUI()
{
	// Could populate from a saved config if we had one
}

// ═══════════════════════════════════════════════════════════════════
// External API: Set folder paths (called from context menu)
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::SetFolderPathsAndRun(const TArray<FString>& FolderPaths, bool bRunAudit)
{
	if (FolderPaths.Num() == 0)
		return;

	// Join all selected folder paths into the FolderPathBox
	FString JoinedPaths;
	for (int32 i = 0; i < FolderPaths.Num(); ++i)
	{
		if (i > 0) JoinedPaths += TEXT(", ");
		JoinedPaths += FolderPaths[i];
	}

	if (FolderPathBox.IsValid())
	{
		FolderPathBox->SetText(FText::FromString(JoinedPaths));
	}

	// Trigger audit or list assets
	if (bRunAudit)
	{
		OnRunAudit();
	}
	else
	{
		OnListAssets();
	}
}

// ═══════════════════════════════════════════════════════════════════
// Go to Asset(s) in Content Browser
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::GoToAsset(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
		return;

	// Sync the asset in Content Browser
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (AssetData.IsValid())
	{
		TArray<FAssetData> AssetsToSync;
		AssetsToSync.Add(AssetData);
		GEditor->SyncBrowserToObjects(AssetsToSync);
	}
}

void SRPDAuditWindow::GoToAsset(TSharedPtr<FRPDAuditIssue> InIssue)
{
	if (!InIssue.IsValid())
		return;

	GoToAsset(InIssue->AssetPath);
}

void SRPDAuditWindow::GoToAssets(const TArray<FString>& AssetPaths)
{
	if (AssetPaths.Num() == 0)
		return;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetsToSync;

	for (const FString& Path : AssetPaths)
	{
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Path));
		if (AssetData.IsValid())
		{
			AssetsToSync.Add(AssetData);
		}
	}

	if (AssetsToSync.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(AssetsToSync);
	}
}


// ═══════════════════════════════════════════════════════════════════
// UI Helpers
// ═══════════════════════════════════════════════════════════════════



// ═══════════════════════════════════════════════════════════════════
// Search / Filter
// ═══════════════════════════════════════════════════════════════════

void SRPDAuditWindow::OnSearchTextChanged(const FText& InText)
{
	SearchFilter = InText.ToString();
	ApplyFilter();
	RefreshResultsList();
}

void SRPDAuditWindow::OnAssetIgnored(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
		return;

	AuditRules::IgnoreAsset(AssetPath);

	// Remove from AllIssues and refresh display
	AllIssues.RemoveAll([&](const TSharedPtr<FRPDAuditIssue>& Issue)
	{
		return Issue.IsValid() && Issue->AssetPath == AssetPath;
	});

	ApplyFilter();
	RefreshResultsList();
}

void SRPDAuditWindow::OnCopyRow(TSharedPtr<FRPDAuditIssue> InIssue)
{
	if (!InIssue.IsValid())
		return;

	FString Text;
	Text += TEXT("Asset: ") + InIssue->AssetName + TEXT("\n");
	Text += TEXT("Path: ") + InIssue->AssetPath + TEXT("\n");
	Text += TEXT("Type: ") + InIssue->AssetType + TEXT("\n");
	Text += TEXT("Rule: ") + InIssue->RuleName + TEXT("\n");
	Text += TEXT("Severity: ");
	switch (InIssue->Severity)
	{
	case ERPDAuditSeverity::Info: Text += TEXT("Info"); break;
	case ERPDAuditSeverity::Warning: Text += TEXT("Warning"); break;
	case ERPDAuditSeverity::Critical: Text += TEXT("Critical"); break;
	case ERPDAuditSeverity::Error: Text += TEXT("Error"); break;
	default: Text += TEXT("Unknown"); break;
	}
	Text += TEXT("\nDetail: ") + InIssue->Detail;

	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

FReply SRPDAuditWindow::OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Ctrl+B — Browse to selected asset(s) in the Content Browser (engine-standard shortcut)
	if (InKeyEvent.GetKey() == EKeys::B && InKeyEvent.IsControlDown())
	{
		GoToSelectedAssets();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
	{
		// Copy selected item
		if (ResultListView.IsValid())
		{
			auto Selected = ResultListView->GetSelectedItems();
			if (Selected.Num() > 0 && Selected[0].IsValid())
			{
				OnCopyRow(Selected[0]);
			}
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SRPDAuditWindow::GoToParentAsset(TSharedPtr<FRPDAuditIssue> InIssue)
{
	if (!InIssue.IsValid())
		return;

	// Prefer the parent's real object path captured during the audit.
	if (!InIssue->ParentPath.IsEmpty())
	{
		GoToAsset(InIssue->ParentPath);
		return;
	}

	// Fallback: the parent may itself be one of the audited assets — match by name.
	if (!InIssue->ParentName.IsEmpty())
	{
		for (const TSharedPtr<FRPDAuditIssue>& Other : AllIssues)
		{
			if (Other.IsValid() && Other->AssetName == InIssue->ParentName)
			{
				GoToAsset(Other->AssetPath);
				return;
			}
		}
	}
}

void SRPDAuditWindow::GoToChildAssets(TSharedPtr<FRPDAuditIssue> InIssue)
{
	if (!InIssue.IsValid())
		return;

	// Children are instances whose parent is THIS asset. Match on the resolved
	// parent path (preferred) or the parent name.
	TArray<FString> ChildPaths;
	for (const TSharedPtr<FRPDAuditIssue>& Other : AllIssues)
	{
		if (!Other.IsValid())
			continue;

		const bool bMatch =
			(!Other->ParentPath.IsEmpty() && Other->ParentPath == InIssue->AssetPath) ||
			(!Other->ParentName.IsEmpty() && Other->ParentName == InIssue->AssetName);
		if (bMatch)
		{
			ChildPaths.AddUnique(Other->AssetPath);
		}
	}

	if (ChildPaths.Num() > 0)
	{
		GoToAssets(ChildPaths);
	}
}

void SRPDAuditWindow::GoToSelectedAssets()
{
	if (!ResultListView.IsValid())
		return;

	TArray<TSharedPtr<FRPDAuditIssue>> Selected = ResultListView->GetSelectedItems();
	TArray<FString> Paths;
	for (const TSharedPtr<FRPDAuditIssue>& Issue : Selected)
	{
		if (Issue.IsValid())
		{
			Paths.AddUnique(Issue->AssetPath);
		}
	}

	GoToAssets(Paths);
}

TSharedPtr<SWidget> SRPDAuditWindow::OnResultsContextMenu()
{
	if (!ResultListView.IsValid())
		return nullptr;

	TArray<TSharedPtr<FRPDAuditIssue>> Selected = ResultListView->GetSelectedItems();
	if (Selected.Num() == 0)
		return nullptr;

	const int32 Count = Selected.Num();
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.AddMenuEntry(
		Count > 1
			? FText::Format(LOCTEXT("GoToSelectedN", "Go to {0} Selected in Content Browser"), FText::AsNumber(Count))
			: LOCTEXT("GoToAssetCtx", "Go to Asset in Content Browser"),
		LOCTEXT("GoToSelectedCtxTip", "Select the highlighted asset(s) in the Content Browser (Ctrl+B)"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &SRPDAuditWindow::GoToSelectedAssets)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyCtx", "Copy"),
		LOCTEXT("CopyCtxTip", "Copy the (first) selected row to clipboard (Ctrl+C)"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Selected]()
		{
			if (Selected.Num() > 0 && Selected[0].IsValid())
				OnCopyRow(Selected[0]);
		})));

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		Count > 1 ? LOCTEXT("IgnoreSelectedCtx", "Ignore Selected Assets") : LOCTEXT("IgnoreCtx", "Ignore This Asset"),
		LOCTEXT("IgnoreCtxTip", "Hide the selected asset(s) from future audits"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, Selected]()
		{
			for (const TSharedPtr<FRPDAuditIssue>& Iss : Selected)
			{
				if (Iss.IsValid())
					OnAssetIgnored(Iss->AssetPath);
			}
		})));

	return MenuBuilder.MakeWidget();
}

void SRPDAuditWindow::ApplyFilter()
{
	FilteredIssues.Empty();

	FString LowerFilter = SearchFilter.ToLower();
	for (const TSharedPtr<FRPDAuditIssue>& Issue : AllIssues)
	{
		if (!Issue.IsValid())
			continue;

		// Severity filter
		bool bPassSeverity = false;
		switch (Issue->Severity)
		{
		case ERPDAuditSeverity::Info: bPassSeverity = bFilterInfo; break;
		case ERPDAuditSeverity::Warning: bPassSeverity = bFilterWarning; break;
		case ERPDAuditSeverity::Critical: bPassSeverity = bFilterCritical; break;
		case ERPDAuditSeverity::Error: bPassSeverity = bFilterError; break;
		default: bPassSeverity = true; break;
		}
		if (!bPassSeverity)
			continue;

		// Search text filter
		if (!SearchFilter.IsEmpty())
		{
			if (!Issue->AssetName.ToLower().Contains(LowerFilter) &&
				!Issue->RuleName.ToLower().Contains(LowerFilter) &&
				!Issue->Detail.ToLower().Contains(LowerFilter) &&
				!Issue->AssetType.ToLower().Contains(LowerFilter))
			{
				continue;
			}
		}

		FilteredIssues.Add(Issue);
	}
}

#undef LOCTEXT_NAMESPACE
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
