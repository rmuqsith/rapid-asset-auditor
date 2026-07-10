// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#include "RPDAuditRules.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"

/** Shared helper: checks a sound asset (USoundWave or USoundCue) for missing Sound Class */
static bool CheckMissingSoundClass(const FAssetData& AssetData, FRPDAuditIssue& OutIssue)
{
	if (USoundWave* Sound = Cast<USoundWave>(AssetData.GetAsset()))
	{
		if (Sound->GetSoundClass() == nullptr)
		{
			OutIssue.Detail = TEXT("No Sound Class assigned.");
			return true;
		}
	}
	if (USoundCue* Cue = Cast<USoundCue>(AssetData.GetAsset()))
	{
		if (Cue->GetSoundClass() == nullptr)
		{
			OutIssue.Detail = TEXT("No Sound Class assigned.");
			return true;
		}
	}
	return false;
}

/** Shared helper: checks missing attenuation on USoundWave or USoundCue */
static bool CheckMissingAttenuation(const FAssetData& AssetData, FRPDAuditIssue& OutIssue)
{
	if (USoundWave* Sound = Cast<USoundWave>(AssetData.GetAsset()))
	{
		if (Sound->AttenuationSettings == nullptr)
		{
			OutIssue.Detail = TEXT("No attenuation — fine for a 2D sound (UI/music/narration); add only if this plays positionally in the world.");
			return true;
		}
	}
	if (USoundCue* Cue = Cast<USoundCue>(AssetData.GetAsset()))
	{
		if (Cue->AttenuationSettings == nullptr)
		{
			OutIssue.Detail = TEXT("No attenuation — fine for a 2D sound (UI/music/narration); add only if this plays positionally in the world.");
			return true;
		}
	}
	return false;
}

FRPDAuditRule AuditRules::Rule_MissingSoundClass = {
	TEXT("MissingSoundClass"),
	TEXT("Sound has no Sound Class assigned — won't respect audio mix categories."),
	NAME_None,
	ERPDAuditSeverity::Warning, 6,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		return CheckMissingSoundClass(AssetData, OutIssue);
	}
};

FRPDAuditRule AuditRules::Rule_MissingAttenuation = {
	TEXT("MissingAttenuation"),
	// Advisory only: non-attenuated is CORRECT for 2D sounds (UI, music, narration).
	// Only positional/world sounds need attenuation, so this is Info, not a defect.
	TEXT("Sound has no attenuation — expected for 2D sounds (UI/music/narration); add only for positional/world sounds."),
	NAME_None,
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		return CheckMissingAttenuation(AssetData, OutIssue);
	}
};

FRPDAuditRule AuditRules::Rule_AudioCompression = {
	TEXT("AudioCompression"),
	TEXT("Audio file may be using uncompressed or inefficient compression format."),
	USoundWave::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USoundWave* Sound = Cast<USoundWave>(AssetData.GetAsset()))
		{
			if (Sound->NumChannels > 2)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%d channels \u2014 multi-channel audio may be uncompressed. Consider using mono/stereo where possible."),
					Sound->NumChannels);
				return true;
			}

			if (Sound->GetDuration() > 60.0f)
			{
				OutIssue.Detail = FString::Printf(
					TEXT("%.1f seconds \u2014 long audio may be uncompressed. Ensure compression is enabled in cook settings."),
					Sound->GetDuration());
				OutIssue.SeverityScore = 3;
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_WAVLength = {
	TEXT("WAVLength"),
	TEXT("Audio file is long — larger memory footprint, consider trimming or streaming."),
	USoundWave::StaticClass()->GetFName(),
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		if (USoundWave* Sound = Cast<USoundWave>(AssetData.GetAsset()))
		{
			float WavLengthWarn = RuleConfig.FindRef(TEXT("WAVLength_Warning"), 10);
			float WavLengthCrit = RuleConfig.FindRef(TEXT("WAVLength_Critical"), 30);
			float Duration = Sound->GetDuration();
			if (Duration > WavLengthCrit)
			{
				OutIssue.Detail = FString::Printf(TEXT("%.1f seconds. Consider streaming or trimming."), Duration);
				OutIssue.SeverityScore = 4;
				return true;
			}
			if (Duration > WavLengthWarn)
			{
				OutIssue.Detail = FString::Printf(TEXT("%.1f seconds. Verify compression is adequate."), Duration);
				return true;
			}
		}
		return false;
	}
};

FRPDAuditRule AuditRules::Rule_NoConcurrencySettings = {
	TEXT("NoConcurrencySettings"),
	// Advisory only: concurrency is for sounds that can STACK (footsteps, impacts,
	// weapon fire). Most sounds don't need it, so this is Info, not a defect.
	TEXT("Sound has no concurrency limit — only relevant for sounds that can stack (footsteps, impacts). Ignore for one-shots/UI/music."),
	NAME_None,
	ERPDAuditSeverity::Info, 2,
	[](const FAssetData& AssetData, FRPDAuditIssue& OutIssue) -> bool
	{
		// GetConcurrencySet() removed in UE5.5. Use ConcurrencySet property directly.
		if (USoundWave* Sound = Cast<USoundWave>(AssetData.GetAsset()))
		{
			if (Sound->ConcurrencySet.Num() == 0)
			{
				OutIssue.Detail = TEXT("No concurrency limit — only needed if this sound can stack (footsteps, impacts, weapon fire); ignore for UI/music/one-shots.");
				return true;
			}
		}
		if (USoundCue* Cue = Cast<USoundCue>(AssetData.GetAsset()))
		{
			if (Cue->ConcurrencySet.Num() == 0 && !Cue->bOverrideConcurrency)
			{
				OutIssue.Detail = TEXT("No concurrency limit — only needed if this sound can stack (footsteps, impacts, weapon fire); ignore for UI/music/one-shots.");
				return true;
			}
		}
		return false;
	}
};
