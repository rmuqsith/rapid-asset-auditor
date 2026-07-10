// Copyright (c) 2026 Rafid Muqsith. Licensed under the MIT License.

#pragma once

#include "Modules/ModuleManager.h"

class FRPDAssetAuditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Register the Asset Auditor dockable tab */
	void RegisterTab();

	/** Spawn the Asset Auditor tab */
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Register the RPD menu bar entry */
	void RegisterMenuEntry();

	/** Add the RPD pull-down menu bar */
	void AddMenuBarEntry(class FMenuBarBuilder& MenuBarBuilder);

	/** Add items under the RPD menu */
	void AddMenuEntry(class FMenuBuilder& MenuBuilder);

	/** Unregister the RPD menu bar entry */
	void UnregisterMenuEntry();

	/** Register Content Browser folder context menu */
	void RegisterContextMenu();

	/** Unregister Content Browser folder context menu */
	void UnregisterContextMenu();

	int32 ContextMenuExtenderHandle;
	TSharedPtr<class FExtender> MenuExtender;
};
