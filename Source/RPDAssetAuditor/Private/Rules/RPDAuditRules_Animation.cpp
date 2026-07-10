// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"

// NOTE: The former Rule_MissingRootMotion was retired. It inferred "has root-bone
// motion" from the mere presence of the RootMotionRootLock asset tag, which is
// present on virtually every sequence — so it flagged almost all animations.
// Detecting genuine latent root motion requires inspecting the root bone's
// translation track via the animation data model (a version-sensitive API).
// Reintroduce it with proper data-model inspection if that check is needed.

FRPDAuditRule AuditRules::Rule_LongAnimSequence = {
	TEXT("LongAnimSequence"),
	TEXT("Animation sequence is very long — may waste memory with unnecessary frames."),
	UAnimSequence::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// Read the real play length from the asset. The old "SequenceLength" asset
		// tag is not reliably present in UE5.5, so the tag-based check silently
		// never fired.
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			const float Length = Anim->GetPlayLength();
			const double AnimLenWarn = RuleConfig.FindRef(TEXT("LongAnimSequence_Warning"), 30);
			if (Length > AnimLenWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%.1f seconds — long sequence. Consider trimming or adjusting key frames."), Length);
				return true;
			}
		}
		return false;
	}
};
