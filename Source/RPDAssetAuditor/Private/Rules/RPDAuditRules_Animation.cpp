// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimCompressionTypes.h"
#include "UObject/UObjectIterator.h"

// NOTE:
// - Notifies/Curves access changed in UE5.x series. In UE5.5 the data is on the
//   raw animation data (CompressedData) which may not be loaded until the asset
//   is cooked. The approaches below use the available API conservatively.
// - Rule_MissingRootMotion was retired (see comment in earlier version).

// ────────────────────────────────────────────────────────────────────────────
// Animation Rule 1 — Long Sequence
// ────────────────────────────────────────────────────────────────────────────
// Very long sequences may carry unnecessary frames and waste memory.
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

// ────────────────────────────────────────────────────────────────────────────
// Animation Rule 2 — High Notify Count
// ────────────────────────────────────────────────────────────────────────────
// Too many notifies in a sequence can cause per-frame overhead when evaluating.
FRPDAuditRule AuditRules::Rule_HighNotifyCount = {
	TEXT("HighNotifyCount"),
	TEXT("Animation has many notifies — evaluate cost per frame."),
	UAnimSequence::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		const int32 NotifyWarn = RuleConfig.FindRef(TEXT("NotifyCount_Warning"), 10);
		const int32 NotifyCrit = RuleConfig.FindRef(TEXT("NotifyCount_Critical"), 25);

		// Prefer the asset-registry tag "NumNotifies" — read straight off FAssetData,
		// no asset load. Only fall back to loading when the tag is absent.
		int32 NotifyCount = -1;
		FString NumNotifiesStr;
		if (AssetData.GetTagValue(TEXT("NumNotifies"), NumNotifiesStr))
		{
			NotifyCount = FCString::Atoi(*NumNotifiesStr);
		}
		else if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			// Direct property access — less reliable across engine versions.
			static FName NotifiesPropName = TEXT("Notifies");
			if (FArrayProperty* NotifiesProp = FindFProperty<FArrayProperty>(Anim->GetClass(), NotifiesPropName))
			{
				FScriptArrayHelper NotifyArray(NotifiesProp, NotifiesProp->ContainerPtrToValuePtr<void>(Anim));
				NotifyCount = NotifyArray.Num();
			}
		}

		if (NotifyCount > NotifyCrit)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d notifies — high evaluation overhead per frame."), NotifyCount);
			return true;
		}
		if (NotifyCount > NotifyWarn)
		{
			OutIssue.Detail = FString::Printf(TEXT("%d notifies — consider merging or reducing redundant notifies."), NotifyCount);
			OutIssue.SeverityScore = 1;
			return true;
		}
		return false;
	}
};

// ────────────────────────────────────────────────────────────────────────────
// Animation Rule 3 — High Curve Count
// ────────────────────────────────────────────────────────────────────────────
// Many curves increase blending cost, especially on additive layers.
FRPDAuditRule AuditRules::Rule_HighCurveCount = {
	TEXT("HighCurveCount"),
	TEXT("Animation has many curves — blending overhead may increase."),
	UAnimSequence::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// Read the "NumCurves" registry tag straight off FAssetData — no asset load.
		FString NumCurvesStr;
		if (AssetData.GetTagValue(TEXT("NumCurves"), NumCurvesStr))
		{
			int32 CurveCount = FCString::Atoi(*NumCurvesStr);
			int32 CurveWarn = RuleConfig.FindRef(TEXT("CurveCount_Warning"), 20);
			int32 CurveCrit = RuleConfig.FindRef(TEXT("CurveCount_Critical"), 50);

			if (CurveCount > CurveCrit)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d curves — high blending overhead."), CurveCount);
				return true;
			}
			if (CurveCount > CurveWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%d curves — consider if all are necessary."), CurveCount);
				OutIssue.SeverityScore = 1;
				return true;
			}
		}
		return false;
	}
};

// ────────────────────────────────────────────────────────────────────────────
// Animation Rule 4 — Additive Animation Flag
// ────────────────────────────────────────────────────────────────────────────
// Flag sequences that are set as additive — not a defect, just an informational
// marker for inventory purposes.
FRPDAuditRule AuditRules::Rule_AdditiveAnimation = {
	TEXT("AdditiveAnimation"),
	TEXT("Animation sequence is additive — inventory marker (not a defect)."),
	UAnimSequence::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 1,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			if (Anim->IsValidAdditive())
			{
				const FString AdditiveType = Anim->GetAdditiveBasePoseType() == AAT_LocalSpaceBase
					? TEXT("LocalSpace")
					: (Anim->GetAdditiveBasePoseType() == AAT_RotationOffsetMeshSpace
						? TEXT("RotationOffsetMesh")
						: TEXT("Unknown"));

				OutIssue.Detail = FString::Printf(
					TEXT("Additive animation (%s). Verify this is intended as blend input."), *AdditiveType);
				OutIssue.SeverityScore = 1;
				return true;
			}
		}
		return false;
	}
};

// ────────────────────────────────────────────────────────────────────────────
// Animation Rule 5 — Compression Profile / Ratio
// ────────────────────────────────────────────────────────────────────────────
// A rough heuristic: very long animations with few notifies/curves may have been
// imported at full framerate without key reduction — inflating memory.
FRPDAuditRule AuditRules::Rule_AnimationCompression = {
	TEXT("AnimationCompression"),
	TEXT("Animation may have suboptimal compression — check key reduction ratio."),
	UAnimSequence::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 3,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			// Lightweight heuristic: long animation + no rot/pos tracks in registry tags
			// may point to raw import at full rate. This is a soft indicator, not
			// deterministic — we just surface it for the user to glance at.
			FString NumFramesStr;
			AssetData.GetTagValue(TEXT("NumFrames"), NumFramesStr);
			int32 NumFrames = FCString::Atoi(*NumFramesStr);

			float Length = Anim->GetPlayLength();
			int32 WarnThreshold = RuleConfig.FindRef(TEXT("AnimCompression_WarnFramesPerSec"), 30);

			if (NumFrames > 0 && Length > 1.0f && NumFrames > FMath::CeilToInt(Length * WarnThreshold))
			{
				double FPS = static_cast<double>(NumFrames) / Length;
				OutIssue.Detail = FString::Printf(
					TEXT("~%.0f fps at %d frames over %.1f seconds — may benefit from key reduction."),
					FPS, NumFrames, Length);
				OutIssue.SeverityScore = 2;
				return true;
			}
		}
		return false;
	}
};
