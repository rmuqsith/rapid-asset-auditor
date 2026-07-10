// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

/**
 * Modal settings window for RPD Asset Auditor.
 * Manages rule toggles, threshold overrides, naming conventions,
 * config profiles, and ignored assets list.
 */
class SRPDAuditSettingsWindow
{
public:
	/** Open the settings window modally */
	static void Open();

private:
	/** Show a Save As dialog for creating a new profile */
	static FReply ShowSaveAsDialog();

	/** Refresh the ignored assets list view */
	static void RefreshIgnoredList();

	/** Prefix text boxes for naming conventions */
	static TSharedPtr<SEditableTextBox> SMPrefixBox;
	static TSharedPtr<SEditableTextBox> SKPrefixBox;
	static TSharedPtr<SEditableTextBox> TexPrefixBox;
	static TSharedPtr<SEditableTextBox> SoundPrefixBox;
	static TSharedPtr<SEditableTextBox> MatPrefixBox;
	static TSharedPtr<SEditableTextBox> PPMatPrefixBox;
	static TSharedPtr<SEditableTextBox> MIPrefixBox;
	static TSharedPtr<SEditableTextBox> AnimPrefixBox;

	/** Ignored assets list view and data source */
	static TSharedPtr<SListView<TSharedPtr<FString>>> IgnoredListView;
	static TArray<TSharedPtr<FString>> IgnoredItems;

	/** Config profile selector combo (kept so it can be refreshed after save/delete/load) */
	static TSharedPtr<SComboBox<TSharedPtr<FString>>> ProfileComboBox;
};
