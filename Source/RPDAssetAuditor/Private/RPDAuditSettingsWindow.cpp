// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditSettingsWindow.h"
#include "RPDAuditRules.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBar.h"

#define LOCTEXT_NAMESPACE "SRPDAuditSettingsWindow"

// ── Simple Profile Name Input Dialog ──
class SRPDProfileInputDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPDProfileInputDialog) {}
		SLATE_ARGUMENT(FString, DefaultText)
		SLATE_ARGUMENT(FText, Prompt)
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnTextCommitted = InArgs._OnTextCommitted;
		FString Default = InArgs._DefaultText;

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(16)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(STextBlock)
					.Text(InArgs._Prompt)
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SAssignNew(TextBox, SEditableTextBox)
					.Text(FText::FromString(Default))
					.SelectAllTextWhenFocused(true)
					.OnTextCommitted(this, &SRPDProfileInputDialog::OnTextCommittedInternal)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("OK", "OK"))
						.OnClicked_Lambda([this, Default]() {
							FString Name = TextBox->GetText().ToString().TrimStartAndEnd();
							if (Name.IsEmpty()) Name = Default;
							if (OnTextCommitted.IsBound()) OnTextCommitted.Execute(FText::FromString(Name), ETextCommit::OnEnter);
							if (ParentWindow.IsValid()) ParentWindow.Pin()->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([this]() {
							if (ParentWindow.IsValid()) ParentWindow.Pin()->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		];
	}

	void SetParentWindow(TSharedPtr<SWindow> InWindow) { ParentWindow = InWindow; }

private:
	TSharedPtr<SEditableTextBox> TextBox;
	TWeakPtr<SWindow> ParentWindow;
	FOnTextCommitted OnTextCommitted;

	void OnTextCommittedInternal(const FText& InText, ETextCommit::Type CommitType)
	{
		if (OnTextCommitted.IsBound() && (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus))
		{
			FString Name = InText.ToString().TrimStartAndEnd();
			if (!Name.IsEmpty())
			{
				OnTextCommitted.Execute(FText::FromString(Name), CommitType);
				if (ParentWindow.IsValid()) ParentWindow.Pin()->RequestDestroyWindow();
			}
		}
	}
};


TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::SMPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::SKPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::TexPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::SoundPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::MatPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::PPMatPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::MIPrefixBox;
TSharedPtr<SEditableTextBox> SRPDAuditSettingsWindow::AnimPrefixBox;
TSharedPtr<SListView<TSharedPtr<FString>>> SRPDAuditSettingsWindow::IgnoredListView;
TArray<TSharedPtr<FString>> SRPDAuditSettingsWindow::IgnoredItems;
TSharedPtr<SComboBox<TSharedPtr<FString>>> SRPDAuditSettingsWindow::ProfileComboBox;

FReply SRPDAuditSettingsWindow::ShowSaveAsDialog()
{
	TSharedRef<SWindow> InputWindow = SNew(SWindow)
		.Title(LOCTEXT("SaveProfileAs", "Save Profile As"))
		.ClientSize(FVector2D(360, 140))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize);

	TSharedRef<SRPDProfileInputDialog> Dialog = SNew(SRPDProfileInputDialog)
		.DefaultText(TEXT("MyProfile"))
		.Prompt(LOCTEXT("ProfileNamePrompt", "Enter a name for the new profile:"))
		.OnTextCommitted_Lambda([](const FText& InText, ETextCommit::Type) {
			FString Name = InText.ToString().TrimStartAndEnd();
			if (!Name.IsEmpty())
			{
				AuditRules::SaveProfile(Name);
				AuditRules::CurrentProfile = Name;
				if (ProfileComboBox.IsValid()) ProfileComboBox->RefreshOptions();
			}
		});

	Dialog->SetParentWindow(InputWindow);
	InputWindow->SetContent(Dialog);
	FSlateApplication::Get().AddModalWindow(InputWindow, nullptr);
	return FReply::Handled();
}

void SRPDAuditSettingsWindow::RefreshIgnoredList()
{
	IgnoredItems.Empty();
	for (const FString& Path : AuditRules::IgnoredAssetPaths)
	{
		IgnoredItems.Add(MakeShareable(new FString(Path)));
	}
	if (IgnoredListView.IsValid())
		IgnoredListView->RequestListRefresh();
}

void SRPDAuditSettingsWindow::Open()
{
	// Discover saved profiles so the selector is populated on first open.
	AuditRules::ScanProfiles();

	TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
		.Title(LOCTEXT("Title", "Rapid Asset Auditor Settings"))
		.ClientSize(FVector2D(480, 600))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::UserSized);

	// Toggles read live from the config maps so loading a profile is reflected
	// immediately without rebuilding the window.
	auto MakeRuleToggle = [](const FString& RuleName, const FText& Label, const FText& Description = FText::GetEmpty())
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([RuleName]() { return AuditRules::RuleEnabled.FindRef(RuleName, true) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([RuleName](ECheckBoxState State)
			{
				AuditRules::RuleEnabled.Add(RuleName, State == ECheckBoxState::Checked);
			})
			.ToolTipText(Description)
			[
				SNew(STextBlock).Text(Label).Font(FAppStyle::Get().GetFontStyle("NormalFont"))
			];
	};

	auto MakeThresholdBox = [](const FString& ConfigKey, const FText& Label, int32 DefaultVal, int32 MinVal, int32 MaxVal)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[ SNew(STextBlock).Text(Label).Font(FAppStyle::Get().GetFontStyle("NormalFont")) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SSpinBox<int32>)
				.MinValue(MinVal)
				.MaxValue(MaxVal)
				.Value_Lambda([ConfigKey, DefaultVal]() { return AuditRules::RuleConfig.FindRef(ConfigKey, DefaultVal); })
				.OnValueChanged_Lambda([ConfigKey](int32 NewVal)
				{
					AuditRules::RuleConfig.Add(ConfigKey, NewVal);
				})
			];
	};

	SettingsWindow->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// ── Nanite Mode ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Nanite", "Nanite Mode"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return !AuditRules::bNaniteInverted ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State)
					{
						AuditRules::bNaniteInverted = (State != ECheckBoxState::Checked);
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ProjectUsesNanite", "Project uses Nanite"))
						.ToolTipText(LOCTEXT("NaniteTooltip", "When checked: Nanite rules warn if Nanite is OFF (expected).\nWhen unchecked: Nanite rules warn if Nanite is ON (non-Nanite project)."))
						.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					]
				]

				// ── Enabled Rules ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Rules", "Enabled Rules"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("NoLODs"), LOCTEXT("Rule_NoLODs", "StaticMesh: Missing LODs"), LOCTEXT("Rule_NoLODs_Desc", "Warns if a StaticMesh has no LOD chain, causing full-detail rendering at all distances.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("NoLODsAndNaniteDisabled"), LOCTEXT("Rule_NoLODsNanite", "StaticMesh: No LODs & no Nanite"), LOCTEXT("Rule_NoLODsNanite_Desc", "StaticMesh with no LODs and Nanite disabled — high GPU cost with no fallback.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("HighVertexCount"), LOCTEXT("Rule_HighVerts", "StaticMesh: High Vertex Count"), LOCTEXT("Rule_HighVerts_Desc", "Flags meshes with many vertices and no LOD reduction. Adjust threshold in Rule Thresholds.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("HighMaterialCount"), LOCTEXT("Rule_HighMats", "StaticMesh: High Material Count"), LOCTEXT("Rule_HighMats_Desc", "Many material slots increase draw calls. Check if materials can be merged.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MissingCollision"), LOCTEXT("Rule_Collision", "StaticMesh: Missing Collision"), LOCTEXT("Rule_Collision_Desc", "StaticMeshes without simple collision can affect physics and tracing performance.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("SameMaterialMultipleSlots"), LOCTEXT("Rule_DupMat", "StaticMesh: Duplicate Material Slots"), LOCTEXT("Rule_DupMat_Desc", "Same material assigned to multiple slots — can be optimized by collapsing slots.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("SkeletalMesh_NoLODs"), LOCTEXT("Rule_SkelNoLOD", "SkeletalMesh: Missing LODs"), LOCTEXT("Rule_SkelNoLOD_Desc", "SkeletalMeshes without LODs render at full detail at all distances.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("SkeletalMesh_HighVertexCount"), LOCTEXT("Rule_SkelVerts", "SkeletalMesh: High Vertex Count"), LOCTEXT("Rule_SkelVerts_Desc", "High vertex count skeletal mesh with no LOD reduction.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("SkeletalMesh_HighMaterialCount"), LOCTEXT("Rule_SkelMats", "SkeletalMesh: High Material Count"), LOCTEXT("Rule_SkelMats_Desc", "High material count on skeletal mesh means many draw calls.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MissingPhysicsAsset"), LOCTEXT("Rule_PhysAsset", "SkeletalMesh: Missing Physics Asset"), LOCTEXT("Rule_PhysAsset_Desc", "SkeletalMeshes without Physics Asset cannot have physical collisions.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("NonPowerOfTwo"), LOCTEXT("Rule_NPOT", "Texture: Non-Power of Two"), LOCTEXT("Rule_NPOT_Desc", "Non-power-of-two textures may have padding/memory issues on some platforms.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("LargeTexture"), LOCTEXT("Rule_LargeTex", "Texture: Large Resolution"), LOCTEXT("Rule_LargeTex_Desc", "Very large texture resolution may waste GPU memory. Adjust thresholds in Rule Thresholds.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("VirtualTextureMismatch"), LOCTEXT("Rule_VT", "Texture: Virtual Texture Mismatch"), LOCTEXT("Rule_VT_Desc", "Texture streaming settings may be suboptimal for this texture usage.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MissingSoundClass"), LOCTEXT("Rule_SndClass", "Sound: Missing Sound Class"), LOCTEXT("Rule_SndClass_Desc", "Sound without Sound Class may not respect audio volume mixing.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MissingAttenuation"), LOCTEXT("Rule_Attn", "Sound: Missing Attenuation"), LOCTEXT("Rule_Attn_Desc", "Sound without Attenuation Asset may not attenuate properly over distance.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("UnusedAsset"), LOCTEXT("Rule_Unused", "Misc: Unused Asset"), LOCTEXT("Rule_Unused_Desc", "Assets not referenced by anything in the project. Candidates for cleanup.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("LongAnimSequence"), LOCTEXT("Rule_LongAnim", "Animation: Long Sequence"), LOCTEXT("Rule_LongAnim_Desc", "Very long animation sequences may have unnecessary frames. Adjust threshold in Rule Thresholds.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MasterMaterialBloat"), LOCTEXT("Rule_MatBloat", "Material: Master Material Bloat"), LOCTEXT("Rule_MatBloat_Desc", "Master materials with many parameters increase compilation time and complexity.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MaterialInstanceBloat"), LOCTEXT("Rule_InstBloat", "Material: Unused Empty Instance"), LOCTEXT("Rule_InstBloat_Desc", "Unused instance that overrides nothing — deletable cruft. A used zero-override instance is valid instance-on-mesh practice and is not flagged.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("MaterialInstanceMinimalOverrides"), LOCTEXT("Rule_MinOver", "Material: Minimal Overrides"), LOCTEXT("Rule_MinOver_Desc", "Material instances with few or no overrides may be unnecessary. Consider using the parent directly.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("BlendModeMismatch"), LOCTEXT("Rule_Blend", "Material: Blend Mode Mismatch"), LOCTEXT("Rule_Blend_Desc", "Material Instance blend mode differs from its parent — unintentional override?")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("ShadingModelMismatch"), LOCTEXT("Rule_Shade", "Material: Shading Model Mismatch"), LOCTEXT("Rule_Shade_Desc", "Material Instance shading model differs from parent — verify this is intentional.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[ MakeRuleToggle(TEXT("HighTextureSampling"), LOCTEXT("Rule_HTS", "Material: High Texture Sampling"), LOCTEXT("Rule_HTS_Desc", "Materials sampling many unique textures increase GPU sampler usage. Adjust threshold in Rule Thresholds.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("OneOffMaterial"), LOCTEXT("Rule_OneOff", "Material: One-Off Master"), LOCTEXT("Rule_OneOff_Desc", "Master material with no instances used by a single mesh — fuels material sprawl.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("DuplicateMaterialInstance"), LOCTEXT("Rule_DupMI", "Material: Duplicate Instance"), LOCTEXT("Rule_DupMI_Desc", "Instance with identical overrides to a sibling of the same parent — redundant, safe to merge.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("DirectMasterMaterialOnStaticMesh"), LOCTEXT("Rule_DirectSM", "Mesh: Direct Master (Static)"), LOCTEXT("Rule_DirectSM_Desc", "Static mesh slot uses a master material directly instead of an instance.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeRuleToggle(TEXT("DirectMasterMaterialOnSkeletalMesh"), LOCTEXT("Rule_DirectSK", "Mesh: Direct Master (Skeletal)"), LOCTEXT("Rule_DirectSK_Desc", "Skeletal mesh slot uses a master material directly instead of an instance.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[ MakeRuleToggle(TEXT("UnusedMasterParameters"), LOCTEXT("Rule_UnusedParams", "Material: Unused Master Params"), LOCTEXT("Rule_UnusedParams_Desc", "Master exposes parameters no instance ever overrides — dead bloat to trim.")) ]

				// ── Platform Presets ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Presets", "Platform Presets"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PresetsHint", "One click sets every threshold + Nanite mode for a target platform. Review below, then Save."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PresetMobile", "Mobile / VR"))
						.ToolTipText(LOCTEXT("PresetMobileTip", "Tight budgets for mobile / Quest VR; non-Nanite."))
						.OnClicked_Lambda([]() -> FReply { AuditRules::ApplyPreset(TEXT("Mobile / VR")); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PresetConsole", "Console"))
						.ToolTipText(LOCTEXT("PresetConsoleTip", "Mid-range budgets; Nanite allowed."))
						.OnClicked_Lambda([]() -> FReply { AuditRules::ApplyPreset(TEXT("Console")); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("PresetPC", "PC"))
						.ToolTipText(LOCTEXT("PresetPCTip", "Most permissive budgets; Nanite allowed."))
						.OnClicked_Lambda([]() -> FReply { AuditRules::ApplyPreset(TEXT("PC")); return FReply::Handled(); })
					]
				]

				// ── Rule Thresholds ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Thresholds", "Rule Thresholds"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighTextureSampling_Warning"), LOCTEXT("HTS_Warn", "HighTexSamp Warning >"), 6, 1, 50) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighTextureSampling_Critical"), LOCTEXT("HTS_Crit", "HighTexSamp Critical >"), 9, 1, 50) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("LargeTexture_Warning"), LOCTEXT("LargeTex_Warn", "LargeTex Warning > px"), 4096, 256, 16384) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("LargeTexture_Critical"), LOCTEXT("LargeTex_Crit", "LargeTex Critical > px"), 8192, 256, 16384) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighVertexCount_Warning"), LOCTEXT("Verts_Warn", "VertCount Warning >"), 5000, 100, 100000) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighVertexCount_Critical"), LOCTEXT("Verts_Crit", "VertCount Critical >"), 10000, 100, 100000) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("NoLODs_MinVerts"), LOCTEXT("NoLODsMin", "NoLODs ignored below verts"), 1000, 0, 100000) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("MasterMaterial_LowReuseMax"), LOCTEXT("MasterMatReuse", "Master consolidation: flag if instances <="), 2, 1, 20) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighMaterialCount_Warning"), LOCTEXT("MatCount_Warn", "MatCount Warning >"), 5, 1, 20) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("HighMaterialCount_Critical"), LOCTEXT("MatCount_Crit", "MatCount Critical >"), 8, 1, 20) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("SkeletalVertex_Warning"), LOCTEXT("SkelVerts_Warn", "SkelVerts Warning >"), 10000, 100, 100000) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("SkeletalVertex_Critical"), LOCTEXT("SkelVerts_Crit", "SkelVerts Critical >"), 20000, 100, 100000) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("SkeletalMatCount_Warning"), LOCTEXT("SkelMats_Warn", "SkelMats Warning >"), 5, 1, 20) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[ MakeThresholdBox(TEXT("LongAnimSequence_Warning"), LOCTEXT("LongAnim_Warn", "AnimLength Warning > sec"), 30, 1, 300) ]

				// ── Naming Conventions ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Naming", "Naming Conventions (Prefixes)"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NamingNote", "Required filename prefix per asset type. Assets whose name doesn't start with the prefix are flagged."))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameSM", "Static Mesh")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(SMPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("SM_")).IsEmpty() ? TEXT("SM_") : *AuditRules::NamingPrefixes.FindRef(TEXT("SM_")))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameSK", "Skeletal Mesh")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(SKPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("SK_")).IsEmpty() ? TEXT("SK_") : *AuditRules::NamingPrefixes.FindRef(TEXT("SK_")))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameT", "Texture")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(TexPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("T_")).IsEmpty() ? TEXT("T_") : *AuditRules::NamingPrefixes.FindRef(TEXT("T_")))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameS", "Sound")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(SoundPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("S_")).IsEmpty() ? TEXT("S_") : *AuditRules::NamingPrefixes.FindRef(TEXT("S_")))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameM", "Material")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(MatPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("M_")).IsEmpty() ? TEXT("M_") : *AuditRules::NamingPrefixes.FindRef(TEXT("M_")))) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[ SNew(STextBlock).Text(LOCTEXT("NamePPM", "Post-Process Material")).MinDesiredWidth(130.0f) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[ SAssignNew(PPMatPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("PPM_")).IsEmpty() ? TEXT("PPM_") : *AuditRules::NamingPrefixes.FindRef(TEXT("PPM_")))) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[ SNew(STextBlock).Text(LOCTEXT("NameMI", "Material Instance")).MinDesiredWidth(130.0f) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[ SAssignNew(MIPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("MI_")).IsEmpty() ? TEXT("MI_") : *AuditRules::NamingPrefixes.FindRef(TEXT("MI_")))) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("NameAnim", "Animation")).MinDesiredWidth(130.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[ SAssignNew(AnimPrefixBox, SEditableTextBox).Text(FText::FromString(AuditRules::NamingPrefixes.FindRef(TEXT("A_")).IsEmpty() ? TEXT("AS_") : *AuditRules::NamingPrefixes.FindRef(TEXT("A_")))) ]
				]

				// ── Config Profiles ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ConfigProfiles", "Config Profiles"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SAssignNew(ProfileComboBox, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&AuditRules::ProfileList)
						.OnSelectionChanged_Lambda([](TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo) {
							// Ignore the programmatic selection that fires during construction.
							if (Item.IsValid() && SelectInfo != ESelectInfo::Direct)
							{
								AuditRules::LoadProfile(*Item);
							}
						})
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
							return SNew(STextBlock).Text(FText::FromString(*Item));
						})
						.ContentPadding(FMargin(6, 2))
						[
							SNew(STextBlock)
							.Text_Lambda([]() { return AuditRules::CurrentProfile.IsEmpty() ? LOCTEXT("DefaultProfile", "_default") : FText::FromString(AuditRules::CurrentProfile); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SaveProfile", "Save Profile"))
						.OnClicked_Lambda([]() -> FReply {
							// If on a named profile, save directly
							FString Name = AuditRules::CurrentProfile;
							if (!Name.IsEmpty() && Name != TEXT("_default"))
							{
								AuditRules::SaveProfile(Name);
								if (ProfileComboBox.IsValid()) ProfileComboBox->RefreshOptions();
								return FReply::Handled();
							}
							// Otherwise show Save As dialog
							return ShowSaveAsDialog();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("DeleteProfile", "Delete"))
						.ToolTipText(LOCTEXT("DeleteProfileTooltip", "Delete the currently selected profile"))
						.OnClicked_Lambda([]() -> FReply {
							FString Name = AuditRules::CurrentProfile;
							if (Name.IsEmpty() || Name == TEXT("_default"))
								return FReply::Handled();
							EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo,
								FText::Format(LOCTEXT("DeleteProfileConfirm", "Delete profile '{0}'?\nThis cannot be undone."), FText::FromString(Name)));
							if (Choice != EAppReturnType::Yes)
								return FReply::Handled();
							AuditRules::DeleteProfile(Name);
							if (ProfileComboBox.IsValid()) ProfileComboBox->RefreshOptions();
							return FReply::Handled();
						})
					]
				]

				// ── Ignored Assets ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Section_Ignored", "Ignored Assets"))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraLarge"))
					.ToolTipText(LOCTEXT("IgnoredTooltip", "Assets hidden from audit results. Click [X] to unignore."))
				]
				+ SVerticalBox::Slot().AutoHeight().MaxHeight(120)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(4)
					[
						SAssignNew(IgnoredListView, SListView<TSharedPtr<FString>>)
						.ListItemsSource(&IgnoredItems)
						.OnGenerateRow_Lambda([](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
						{
							return SNew(STableRow<TSharedPtr<FString>>, Owner)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(*Item))
									.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
									.ToolTipText(FText::FromString(*Item))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SButton)
									.Text(FText::FromString(TEXT("X")))
									.ToolTipText(LOCTEXT("UnignoreTooltip", "Remove from ignore list"))
									.OnClicked_Lambda([Item]() -> FReply {
										AuditRules::UnignoreAsset(*Item);
										SRPDAuditSettingsWindow::RefreshIgnoredList();
										return FReply::Handled();
									})
								]
							];
						})
						.SelectionMode(ESelectionMode::None)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0).HAlign(HAlign_Right)
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearAllIgnored", "Clear All"))
					.ToolTipText(LOCTEXT("ClearAllIgnoredTooltip", "Remove all assets from ignore list"))
					.OnClicked_Lambda([]() -> FReply {
						TArray<FString> AllIgnored = AuditRules::IgnoredAssetPaths;
						for (const FString& Path : AllIgnored)
						{
							AuditRules::UnignoreAsset(Path);
						}
						SRPDAuditSettingsWindow::RefreshIgnoredList();
						return FReply::Handled();
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IgnoredConfigNote", "Ignored paths are saved in the main config file."))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				// ── Buttons ──
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Save", "Save"))
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.OnClicked_Lambda([WeakWindow = TWeakPtr<SWindow>(SettingsWindow)]() -> FReply
						{
							// Save naming prefixes
							auto SavePrefix = [](const TSharedPtr<SEditableTextBox>& Box, const FString& Key) {
								if (Box.IsValid()) AuditRules::NamingPrefixes.Add(Key, Box->GetText().ToString());
							};
							SavePrefix(SRPDAuditSettingsWindow::SMPrefixBox, TEXT("SM_"));
							SavePrefix(SRPDAuditSettingsWindow::SKPrefixBox, TEXT("SK_"));
							SavePrefix(SRPDAuditSettingsWindow::TexPrefixBox, TEXT("T_"));
							SavePrefix(SRPDAuditSettingsWindow::SoundPrefixBox, TEXT("S_"));
							SavePrefix(SRPDAuditSettingsWindow::MatPrefixBox, TEXT("M_"));
							SavePrefix(SRPDAuditSettingsWindow::PPMatPrefixBox, TEXT("PPM_"));
							SavePrefix(SRPDAuditSettingsWindow::MIPrefixBox, TEXT("MI_"));
							SavePrefix(SRPDAuditSettingsWindow::AnimPrefixBox, TEXT("A_"));

							AuditRules::SaveConfig();

							// If a named profile is active, fold the edits into it too,
							// otherwise the profile would override them on next load.
							if (!AuditRules::CurrentProfile.IsEmpty() && AuditRules::CurrentProfile != TEXT("_default"))
							{
								AuditRules::SaveProfile(AuditRules::CurrentProfile);
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "Cancel"))
						.OnClicked_Lambda([WeakWindow = TWeakPtr<SWindow>(SettingsWindow)]() -> FReply
						{
							AuditRules::LoadConfig();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Reset", "Reset to Defaults"))
						.OnClicked_Lambda([WeakWindow = TWeakPtr<SWindow>(SettingsWindow)]() -> FReply
						{
							EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNo,
								LOCTEXT("ResetConfirm", "Reset all settings to defaults? Any unsaved changes will be lost.\n\nThe window will close and must be reopened."));
							if (Choice != EAppReturnType::Yes)
								return FReply::Handled();

							AuditRules::ResetRuleConfig();
							AuditRules::SaveConfig();
							if (WeakWindow.IsValid())
								WeakWindow.Pin()->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		]
	);

	// Populate ignored assets list
	RefreshIgnoredList();

	// Reflect the active profile in the selector (SetSelectedItem fires with
	// ESelectInfo::Direct, which the handler ignores — so no spurious load).
	{
		const FString ActiveName = AuditRules::CurrentProfile.IsEmpty() ? TEXT("_default") : AuditRules::CurrentProfile;
		for (const TSharedPtr<FString>& P : AuditRules::ProfileList)
		{
			if (P.IsValid() && *P == ActiveName && ProfileComboBox.IsValid())
			{
				ProfileComboBox->SetSelectedItem(P);
				break;
			}
		}
	}

	FSlateApplication::Get().AddModalWindow(SettingsWindow, nullptr);
}

#undef LOCTEXT_NAMESPACE
