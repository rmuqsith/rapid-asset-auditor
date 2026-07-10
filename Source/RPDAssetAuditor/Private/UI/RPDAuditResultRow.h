// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "RPDAuditResult.h"

DECLARE_DELEGATE_OneParam(FOnRowDoubleClicked, TSharedPtr<FRPDAuditIssue>)
DECLARE_DELEGATE_OneParam(FOnAssetIgnored, const FString& /* AssetPath */)
DECLARE_DELEGATE_OneParam(FOnRowCopy, TSharedPtr<FRPDAuditIssue>)
DECLARE_DELEGATE_OneParam(FOnGoToParent, TSharedPtr<FRPDAuditIssue>)
DECLARE_DELEGATE_OneParam(FOnGoToChildren, TSharedPtr<FRPDAuditIssue>)
DECLARE_DELEGATE(FOnGoToSelected)

/** A single row in the audit results list */
class SRPDAuditResultRow : public SMultiColumnTableRow<TSharedPtr<FRPDAuditIssue>>
{
public:
	SLATE_BEGIN_ARGS(SRPDAuditResultRow) {}
	SLATE_ARGUMENT(TSharedPtr<FRPDAuditIssue>, Issue)
		SLATE_EVENT(FOnRowDoubleClicked, OnDoubleClicked)
		SLATE_EVENT(FOnAssetIgnored, OnAssetIgnored)
		SLATE_EVENT(FOnRowCopy, OnCopy)
		SLATE_EVENT(FOnGoToParent, OnGoToParent)
		SLATE_EVENT(FOnGoToChildren, OnGoToChildren)
		SLATE_EVENT(FOnGoToSelected, OnGoToSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTable);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRPDAuditIssue> Issue;
	FOnRowDoubleClicked DoubleClickedDelegate;
	FOnAssetIgnored AssetIgnoredDelegate;
	FOnRowCopy CopyDelegate;
	FOnGoToParent GoToParentDelegate;
	FOnGoToChildren GoToChildrenDelegate;
	FOnGoToSelected GoToSelectedDelegate;

	FReply OnAssetNameClicked();
	FReply OnParentNameClicked();
	FReply OnChildCountClicked();
	FReply OnContextMenuIgnore();
	FReply OnContextMenuCopy();

	FLinearColor GetSeverityColor() const;
	FText GetSeverityIcon() const;

	virtual TSharedPtr<SWidget> GetCustomContextMenu();
};
