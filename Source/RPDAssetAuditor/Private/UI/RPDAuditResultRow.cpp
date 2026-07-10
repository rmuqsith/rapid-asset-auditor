// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "UI/RPDAuditResultRow.h"
#include "RPDAuditRules.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "UObject/EnumProperty.h"
#include "SlateOptMacros.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "SRPDAuditResultRow"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRPDAuditResultRow::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTable)
{
	Issue = Args._Issue;
	DoubleClickedDelegate = Args._OnDoubleClicked;
	AssetIgnoredDelegate = Args._OnAssetIgnored;
	CopyDelegate = Args._OnCopy;
	GoToParentDelegate = Args._OnGoToParent;
	GoToChildrenDelegate = Args._OnGoToChildren;
	GoToSelectedDelegate = Args._OnGoToSelected;

	SMultiColumnTableRow<TSharedPtr<FRPDAuditIssue>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FRPDAuditIssue>>::FArguments()
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
		.Padding(2.0f),
		OwnerTable
	);
}

/** Helper to make a clickable text button with hover feedback */
static TSharedRef<SWidget> MakeClickableText(const FText& InText, const FText& InTooltip,
	FOnClicked OnClicked, const FLinearColor& LinkColor = FLinearColor(0.4f, 0.7f, 1.0f))
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(2, 0))
		.OnClicked(OnClicked)
		.ToolTipText(InTooltip)
		.ForegroundColor(LinkColor)
		[
			SNew(STextBlock)
			.Text(InText)
			.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
		];
}

/** Plain text, no interaction */
static TSharedRef<SWidget> MakePlainText(const FText& InText, const FSlateColor& InColor = FSlateColor::UseForeground(), const FText& InTooltip = FText::GetEmpty())
{
	return SNew(STextBlock)
		.Text(InText)
		.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
		.ColorAndOpacity(InColor)
		.ToolTipText(InTooltip.IsEmpty() ? InText : InTooltip);
}

TSharedRef<SWidget> SRPDAuditResultRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Issue.IsValid())
		return SNullWidget::NullWidget;

	if (ColumnName == TEXT("Severity"))
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 4, 0)
			[
				SNew(SColorBlock)
				.Color(GetSeverityColor())
				.Size(FVector2D(12, 12))
				.UseSRGB(false)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(GetSeverityIcon())
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.11"))
				.ColorAndOpacity(GetSeverityColor())
				.ToolTipText(StaticEnum<ERPDAuditSeverity>()->GetDisplayNameTextByValue((int64)Issue->Severity))
			];
	}
	else if (ColumnName == TEXT("Rule"))
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Issue->RuleName))
			.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			.ToolTipText(FText::FromString(Issue->Detail));
	}
	else if (ColumnName == TEXT("Asset"))
	{
		return MakeClickableText(
			FText::FromString(Issue->AssetName),
			FText::Format(LOCTEXT("AssetTooltip", "Click to select in Content Browser\n{0}"), FText::FromString(Issue->AssetPath)),
			FOnClicked::CreateRaw(this, &SRPDAuditResultRow::OnAssetNameClicked)
		);
	}
	else if (ColumnName == TEXT("Path"))
	{
		// Folder only (asset name stripped) so users can overview where assets live.
		return MakePlainText(
			FText::FromString(Issue->GetAssetFolder()),
			FSlateColor::UseSubduedForeground(),
			FText::FromString(Issue->AssetPath));
	}
	else if (ColumnName == TEXT("Parent"))
	{
		if (Issue->ParentName.IsEmpty())
		{
			return MakePlainText(FText::FromString(TEXT("—")), FSlateColor::UseSubduedForeground());
		}
		return MakeClickableText(
			FText::FromString(Issue->ParentName),
			LOCTEXT("ParentTooltip", "Click to select parent asset in Content Browser"),
			FOnClicked::CreateRaw(this, &SRPDAuditResultRow::OnParentNameClicked)
		);
	}
	else if (ColumnName == TEXT("Type"))
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Issue->AssetType))
			.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
	else if (ColumnName == TEXT("Children"))
	{
		if (Issue->ChildCount >= 0)
		{
			return MakeClickableText(
				FText::AsNumber(Issue->ChildCount),
				FText::Format(LOCTEXT("ChildCountTooltip", "Click to select {0} child(ren) in Content Browser"), FText::AsNumber(Issue->ChildCount)),
				FOnClicked::CreateRaw(this, &SRPDAuditResultRow::OnChildCountClicked),
				Issue->ChildCount > 0 ? FLinearColor(0.4f, 0.7f, 1.0f) : FSlateColor::UseSubduedForeground().GetSpecifiedColor()
			);
		}
		return MakePlainText(FText::FromString(TEXT("—")), FSlateColor::UseSubduedForeground());
	}
	else if (ColumnName == TEXT("Detail"))
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Issue->Detail))
			.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
			.AutoWrapText(true);
	}
	else if (ColumnName == TEXT("Ignore"))
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(0))
			.OnClicked_Raw(this, &SRPDAuditResultRow::OnContextMenuIgnore)
			.ToolTipText(LOCTEXT("IgnoreAsset", "Ignore this asset"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u00d7")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	return SNullWidget::NullWidget;
}

FReply SRPDAuditResultRow::OnAssetNameClicked()
{
	if (Issue.IsValid() && DoubleClickedDelegate.IsBound())
	{
		DoubleClickedDelegate.Execute(Issue);
	}
	return FReply::Handled();
}

FReply SRPDAuditResultRow::OnParentNameClicked()
{
	if (Issue.IsValid() && GoToParentDelegate.IsBound())
	{
		GoToParentDelegate.Execute(Issue);
	}
	return FReply::Handled();
}

FReply SRPDAuditResultRow::OnChildCountClicked()
{
	if (Issue.IsValid() && GoToChildrenDelegate.IsBound())
	{
		GoToChildrenDelegate.Execute(Issue);
	}
	return FReply::Handled();
}

FLinearColor SRPDAuditResultRow::GetSeverityColor() const
{
	if (!Issue.IsValid())
		return FLinearColor::White;

	switch (Issue->Severity)
	{
	case ERPDAuditSeverity::Critical:
	case ERPDAuditSeverity::Error:
		return FLinearColor(1.0f, 0.2f, 0.2f); // Red
	case ERPDAuditSeverity::Warning:
		return FLinearColor(1.0f, 0.7f, 0.1f); // Amber
	default:
		return FLinearColor(0.3f, 0.6f, 1.0f); // Blue
	}
}

FText SRPDAuditResultRow::GetSeverityIcon() const
{
	if (!Issue.IsValid())
		return FText::GetEmpty();

	switch (Issue->Severity)
	{
	case ERPDAuditSeverity::Critical:
	case ERPDAuditSeverity::Error:
		return FText::FromString(TEXT("\uf06a")); // fa-exclamation-triangle
	case ERPDAuditSeverity::Warning:
		return FText::FromString(TEXT("\uf071")); // fa-exclamation-circle
	default:
		return FText::FromString(TEXT("\uf05a")); // fa-info-circle
	}
}

FReply SRPDAuditResultRow::OnContextMenuIgnore()
{
	if (Issue.IsValid() && AssetIgnoredDelegate.IsBound())
	{
		AssetIgnoredDelegate.Execute(Issue->AssetPath);
	}
	return FReply::Handled();
}

TSharedPtr<SWidget> SRPDAuditResultRow::GetCustomContextMenu()
{
	if (!Issue.IsValid())
		return nullptr;

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("GoToAssetMenu", "Go to Asset"),
		LOCTEXT("GoToAssetTooltip", "Select this asset in Content Browser"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnAssetNameClicked(); })));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("GoToSelectedMenu", "Go to Selected in Content Browser"),
		LOCTEXT("GoToSelectedTooltip", "Select every highlighted row's asset in the Content Browser"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { if (GoToSelectedDelegate.IsBound()) GoToSelectedDelegate.Execute(); })));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyRowMenu", "Copy"),
		LOCTEXT("CopyRowTooltip", "Copy this row to clipboard (Ctrl+C)"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnContextMenuCopy(); })));

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("IgnoreAssetMenu", "Ignore This Asset"),
		LOCTEXT("IgnoreAssetTooltip", "Hide this asset from future audits"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { OnContextMenuIgnore(); })));

	return MenuBuilder.MakeWidget();
}

FReply SRPDAuditResultRow::OnContextMenuCopy()
{
	if (Issue.IsValid() && CopyDelegate.IsBound())
	{
		CopyDelegate.Execute(Issue);
	}
	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
