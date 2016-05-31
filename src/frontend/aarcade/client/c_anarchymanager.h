#ifndef C_ANARCHY_MANAGER_H
#define C_ANARCHY_MANAGER_H

#include <KeyValues.h>
#include "c_webmanager.h"

class C_AnarchyManager : public CAutoGameSystemPerFrame
{
public:
	C_AnarchyManager();
	~C_AnarchyManager();

	virtual bool Init();
	virtual void PostInit();
	virtual void Shutdown();

	// Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreClearSteamAPIContext();
	virtual void LevelShutdownPreEntity();
	virtual void LevelShutdownPostEntity();

	virtual void OnSave();
	virtual void OnRestore();
	virtual void SafeRemoveIfDesired();

	virtual bool IsPerFrame();

	// Called before rendering
	virtual void PreRender();

	// Gets called each frame
	virtual void Update(float frametime);

	// Called after rendering
	virtual void PostRender();
	
	void AnarchyBegin();
	bool AttemptSelectEntity();
	bool SelectEntity(C_BaseEntity* pEntity);
	bool DeselectEntity(C_BaseEntity* pEntity);
	void AddGlowEffect(C_BaseEntity* pEntity);
	void RemoveGlowEffect(C_BaseEntity* pEntity);

	// helpers
	std::string GenerateUniqueId();

	// accessors
	C_WebManager* GetWebManager() { return m_pWebManager; }
	C_BaseEntity* GetSelectedEntity() { return m_pSelectedEntity; }
	
private:
	C_WebManager* m_pWebManager;
	C_BaseEntity* m_pSelectedEntity;
	//CEntGlowEffect* m_pEntityGlowEffect;
};

extern C_AnarchyManager* g_pAnarchyManager;

#endif