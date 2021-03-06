#include "cbase.h"

#include "c_anarchymanager.h"
#include "aa_globals.h"
#include "c_metaversemanager.h"
#include "filesystem.h"
#include "vgui/IInput.h"
#include <algorithm>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_MetaverseManager::C_MetaverseManager()
{
	DevMsg("MetaverseManager: Constructor\n");
	m_pWebTab = null;
	m_pPreviousSearchInfo = null;
	m_pPreviousModelSearchInfo = null;
	m_pSpawningObject = null;
	m_pSpawningObjectEntity = null;
	m_iSpawningRotationAxis = 1;
	m_spawningAngles.x = 0;
	m_spawningAngles.y = 0;
	m_spawningAngles.z = 0;

	m_libraryBrowserContextCategory = "items";
	m_libraryBrowserContextId = "";
	m_libraryBrowserContextSearch = "";
	m_libraryBrowserContextFilter = "";
	
	m_pPreviousLoadLocalItemLegacyBackpack = null;

	m_defaultFields.push_back("title");
	m_defaultFields.push_back("description");
	m_defaultFields.push_back("file");
	m_defaultFields.push_back("type");
	m_defaultFields.push_back("app");
	m_defaultFields.push_back("reference");
	m_defaultFields.push_back("preview");
	m_defaultFields.push_back("download");
	m_defaultFields.push_back("stream");
	m_defaultFields.push_back("screen");
	m_defaultFields.push_back("marquee");
	m_defaultFields.push_back("model");
	
	m_db = null;
}

C_MetaverseManager::~C_MetaverseManager()
{
	DevMsg("MetaverseManager: Destructor\n");

	m_mapScreenshots.clear();

	if (m_previousLoadLocalAppFilePath != "")
	{
		g_pFullFileSystem->FindClose(m_previousLoadLocalAppFileHandle);
		m_previousLoadLocalAppFilePath = "";
	}

	if (m_pPreviousSearchInfo)
		m_pPreviousSearchInfo->deleteThis();

	if (m_pPreviousModelSearchInfo)
		m_pPreviousModelSearchInfo->deleteThis();

	// m_apps
	while (!m_apps.empty())
	{
		m_apps.begin()->second->deleteThis();
		m_apps.erase(m_apps.begin());
	}

	// m_models
	while (!m_models.empty())
	{
		m_models.begin()->second->deleteThis();
		m_models.erase(m_models.begin());
	}

	// m_items
	while (!m_items.empty())
	{
		m_items.begin()->second->deleteThis();
		m_items.erase(m_items.begin());
	}

	// m_types
	while (!m_types.empty())
	{
		m_types.begin()->second->deleteThis();
		m_types.erase(m_types.begin());
	}

	// screenshots
	while (!m_mapScreenshots.empty())
	{
		m_mapScreenshots.begin()->second->deleteThis();
		m_mapScreenshots.erase(m_mapScreenshots.begin());
	}

	sqlite3_close(m_db);
	// TODO: error objects for sqlite must also be cleaned up
}

bool C_MetaverseManager::CreateDb(std::string libraryFile, sqlite3** pDb)
{
	// create or open the library.db
	DevMsg("Opening (or creating) SQL DB at: %s\n", libraryFile.c_str());

	sqlite3* db;
	int rc = sqlite3_open(libraryFile.c_str(), &db);
	if (!db)
	{
		DevMsg("Critical error opening the specified SQLite3 database!\n");
		return false;
	}

	*pDb = db;
	return true;
}

bool C_MetaverseManager::IsEmptyDb(sqlite3** pDb)
{
	if (!pDb)
		pDb = &m_db;

	// no maps means empty library
	// TODO: Improve this check
	// confirm that default stuff exists
	bool bNeedsDefault = true;

	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(*pDb, "SELECT * from  types", -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		bNeedsDefault = false;
		/*
		int length;
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			length = sqlite3_column_bytes(stmt, 1);

			if (length == 0)
			{
				DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
				continue;
			}

			//KeyValues* pMap = new KeyValues("map");

			//CUtlBuffer buf(0, length, 0);
			//buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
			//pMap->ReadAsBinary(buf);

			// TODO: Look up any alias here first!! (or maybe later.  later is probably OK.  later is probably better. way later.  later during library functions.)
			//pMap = this->GetActiveKeyValues(pMap);
			//if (pMap)
			//	bNeedsDefault = false;

			break;
		}
		*/
		sqlite3_finalize(stmt);
	}

	return bNeedsDefault;
}

void C_MetaverseManager::AddDefaultTables(sqlite3** pDb)
{
	if (!pDb)
		pDb = &m_db;

	// create the tables
	char *error;
	const char *sqlCreateAppsTable = "CREATE TABLE apps (id STRING PRIMARY KEY, value BLOB);";
	int rc = sqlite3_exec(*pDb, sqlCreateAppsTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateItemsTable = "CREATE TABLE items (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreateItemsTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateMapsTable = "CREATE TABLE maps (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreateMapsTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateModelsTable = "CREATE TABLE models (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreateModelsTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreatePlatformsTable = "CREATE TABLE platforms (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreatePlatformsTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateTypesTable = "CREATE TABLE types (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreateTypesTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateInstancesTable = "CREATE TABLE instances (id STRING PRIMARY KEY, value BLOB);";
	rc = sqlite3_exec(*pDb, sqlCreateInstancesTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	const char *sqlCreateVersionTable = "CREATE TABLE version (id INTEGER PRIMARY KEY, value INTEGER);";
	rc = sqlite3_exec(*pDb, sqlCreateVersionTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(*pDb));
		sqlite3_free(error);
	}

	// now save our AA_LIBRARY_VERSION number to the DB for future proofing
	sqlite3_stmt *stmt = NULL;
	rc = sqlite3_prepare(*pDb, VarArgs("INSERT INTO version (id, value) VALUES(0, %i)", AA_LIBRARY_VERSION), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(*pDb));

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
		DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(*pDb));

	sqlite3_finalize(stmt);
}

addDefaultLibraryContext_t* C_MetaverseManager::GetAddDefaultLibraryContext()
{
	auto it = m_addDefaultLibraryContexts.begin();
	if( it != m_addDefaultLibraryContexts.end() )
		return it->first;
	
	return null;
}

void C_MetaverseManager::SetAddDefaultLibraryToDbIterativeContext(addDefaultLibraryContext_t* pContext)
{
	auto it = m_addDefaultLibraryContexts.find(pContext);
	if (it == m_addDefaultLibraryContexts.end())
		m_addDefaultLibraryContexts[pContext] = true;
}

bool C_MetaverseManager::DeleteAddDefaultLibraryContext(addDefaultLibraryContext_t* pContext)
{
	auto it = m_addDefaultLibraryContexts.find(pContext);
	if (it != m_addDefaultLibraryContexts.end())
	{
		m_addDefaultLibraryContexts.erase(it);
		delete pContext;
		return true;
	}

	return false;
}

/* STATES
	0 - uninitialized
	1 - finished
	2 - adding apps
	3 - adding cabinets
	4 - adding maps
	5 - adding models
	6 - adding types
*/
void C_MetaverseManager::AddDefaultLibraryToDbIterative(addDefaultLibraryContext_t* pContext)
{
	sqlite3** pDb = (pContext->pDb) ? pContext->pDb : &m_db;

	// get ready for apps
	if (pContext->state == 0)
	{
		pContext->pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\apps\\*.txt", "MOD", &pContext->handle);
		pContext->state = 2;
	}

	if (pContext->state == 2)
	{
		// 2 APPS
		while (pContext->pFilename != NULL && g_pFullFileSystem->FindIsDirectory(pContext->handle))
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);

		if (pContext->pFilename != NULL)
		{
			pContext->kv = new KeyValues("app");
			if (pContext->kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\apps\\%s", pContext->pFilename), "MOD"))
			{
				KeyValues* active = this->GetActiveKeyValues(pContext->kv);
				this->SaveSQL(pDb, "apps", active->GetString("info/id"), pContext->kv);
			}
			pContext->numApps++;
			pContext->kv->deleteThis();
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);
		}
		else
		{
			g_pFullFileSystem->FindClose(pContext->handle);

			// get ready for cabinets
			pContext->pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\cabinets\\*.txt", "MOD", &pContext->handle);
			pContext->state = 3;
		}
	}

	if (pContext->state == 3)
	{
		// 3 CABINETS
		while (pContext->pFilename != NULL && g_pFullFileSystem->FindIsDirectory(pContext->handle))
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);

		if (pContext->pFilename != NULL)
		{
			pContext->kv = new KeyValues("model");
			if (pContext->kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\cabinets\\%s", pContext->pFilename), "MOD"))
			{
				KeyValues* active = this->GetActiveKeyValues(pContext->kv);

				//std::string id = active->GetString("info/id");
				//if (id.find_first_of(".") != std::string::npos)
					//id = g_pAnarchyManager->GenerateUniqueId();

				//DevMsg("Saving cabinet w/ title %s and id %s\n", active->GetString("title"), active->GetString("info/id"));
				// generate an ID on the file name instead of trying to load the one saved in the txt file cuz of how shiddy KV's are at loading from txt files.
				std::string id = g_pAnarchyManager->GenerateLegacyHash(active->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID)));
				active->SetString("info/id", id.c_str());
				this->SaveSQL(pDb, "models", id.c_str(), pContext->kv);
			}
			pContext->numCabinets++;
			pContext->kv->deleteThis();
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);
		}
		else
		{
			g_pFullFileSystem->FindClose(pContext->handle);

			// get ready for maps
			pContext->pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\maps\\*.txt", "MOD", &pContext->handle);
			pContext->state = 4;
		}
	}

	if (pContext->state == 4)
	{
		// 4 MAPS
		while (pContext->pFilename != NULL && g_pFullFileSystem->FindIsDirectory(pContext->handle))
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);

		if (pContext->pFilename != NULL)
		{
			pContext->kv = new KeyValues("map");
			if (pContext->kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\maps\\%s", pContext->pFilename), "MOD"))
			{
				KeyValues* active = this->GetActiveKeyValues(pContext->kv);
				this->SaveSQL(pDb, "maps", active->GetString("info/id"), pContext->kv);
			}
			pContext->numMaps++;
			pContext->kv->deleteThis();
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);
		}
		else
		{
			g_pFullFileSystem->FindClose(pContext->handle);

			// get ready for models
			pContext->pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\models\\*.txt", "MOD", &pContext->handle);
			pContext->state = 5;
		}
	}

	if (pContext->state == 5)
	{
		// 5 MODELS
		while (pContext->pFilename != NULL && g_pFullFileSystem->FindIsDirectory(pContext->handle))
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);

		if (pContext->pFilename != NULL)
		{
			pContext->kv = new KeyValues("model");
			if (pContext->kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\models\\%s", pContext->pFilename), "MOD"))
			{
				KeyValues* active = this->GetActiveKeyValues(pContext->kv);
				this->SaveSQL(pDb, "models", active->GetString("info/id"), pContext->kv);
			}
			pContext->numModels++;
			pContext->kv->deleteThis();
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);
		}
		else
		{
			g_pFullFileSystem->FindClose(pContext->handle);

			// get ready for types
			pContext->pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\types\\*.txt", "MOD", &pContext->handle);
			pContext->state = 6;
		}
	}

	if (pContext->state == 6)
	{
		// 6 TYPES
		while (pContext->pFilename != NULL && g_pFullFileSystem->FindIsDirectory(pContext->handle))
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);

		if (pContext->pFilename != NULL)
		{
			pContext->kv = new KeyValues("type");
			if (pContext->kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\types\\%s", pContext->pFilename), "MOD"))
			{
				KeyValues* active = this->GetActiveKeyValues(pContext->kv);
				this->SaveSQL(pDb, "types", active->GetString("info/id"), pContext->kv);
			}
			pContext->numTypes++;
			pContext->kv->deleteThis();
			pContext->pFilename = g_pFullFileSystem->FindNext(pContext->handle);
		}
		else
		{
			g_pFullFileSystem->FindClose(pContext->handle);

			// finished
			pContext->state = 1;
		}
	}
}

void C_MetaverseManager::AddDefaultLibraryToDb(unsigned int& numApps, unsigned int& numCabinets, unsigned int& numModels, unsigned int& numTypes, sqlite3** pDb)
{
	// OBSOLETE!!
	if (!pDb)
		pDb = &m_db;

	// NOW LOAD IN ALL DEFAULTLIBRARY KEYVALUES & ADD THEM TO THE LIBRARY (and copy scraper.js files into the right folder)
	FileFindHandle_t handle;
	const char *pFilename;
	KeyValues* kv;

	// APPS
	pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\apps\\*.txt", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		kv = new KeyValues("app");
		if (kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\apps\\%s", pFilename), "MOD"))
		{
			KeyValues* active = this->GetActiveKeyValues(kv);
			this->SaveSQL(pDb, "apps", active->GetString("info/id"), kv);
			numApps++;
		}
		kv->deleteThis();

		pFilename = g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);

	// CABINETS
	pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\cabinets\\*.txt", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		kv = new KeyValues("model");
		if (kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\cabinets\\%s", pFilename), "MOD"))
		{
			KeyValues* active = this->GetActiveKeyValues(kv);
			DevMsg("Saving cabinet w/ title %s and id %s\n", active->GetString("title"), active->GetString("info/id"));
			this->SaveSQL(pDb, "models", active->GetString("info/id"), kv);
			numCabinets++;
		}
		kv->deleteThis();

		pFilename = g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);

	// MAPS
	pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\maps\\*.txt", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		kv = new KeyValues("map");
		if (kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\maps\\%s", pFilename), "MOD"))
		{
			KeyValues* active = this->GetActiveKeyValues(kv);
			this->SaveSQL(pDb, "maps", active->GetString("info/id"), kv);
		}
		kv->deleteThis();

		pFilename = g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);

	// MODELS
	pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\models\\*.txt", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		kv = new KeyValues("model");
		if (kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\models\\%s", pFilename), "MOD"))
		{
			KeyValues* active = this->GetActiveKeyValues(kv);
			this->SaveSQL(pDb, "models", active->GetString("info/id"), kv);
			numModels++;
		}
		kv->deleteThis();

		pFilename = g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);

	// TYPES
	pFilename = g_pFullFileSystem->FindFirstEx("defaultLibrary\\types\\*.txt", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		kv = new KeyValues("type");
		if (kv->LoadFromFile(g_pFullFileSystem, VarArgs("defaultLibrary\\types\\%s", pFilename), "MOD"))
		{
			KeyValues* active = this->GetActiveKeyValues(kv);
			this->SaveSQL(pDb, "types", active->GetString("info/id"), kv);
			numTypes++;
		}
		kv->deleteThis();

		pFilename = g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);

	// SCRAPERS
	// TODO: work
}

void C_MetaverseManager::Init()
{
	this->CreateDb("aarcade_user/library.db", &m_db);
}

int C_MetaverseManager::ExtractLibraryVersion(sqlite3** pDb)
{
	if (!pDb)
		pDb = &m_db;

	int value = -1;

	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(*pDb, "SELECT * from version WHERE id = 0", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(*pDb));
	else if (sqlite3_step(stmt) == SQLITE_ROW)
		value = sqlite3_column_int(stmt, 1);

	sqlite3_finalize(stmt);
	return value;
}

/*
void C_MetaverseManager::OnWebTabCreated(C_WebTab* pWebTab)
{
	m_pWebTab = pWebTab;
}
*/

bool C_MetaverseManager::ConvertLibraryVersion(unsigned int uOld, unsigned int uTarget)
{
	// 1. Make a backup of library.db to aarcade_user/backups/library-auto0.db
	// 2. The library is already loaded, so it just needs to be converted right now.
	// 3. The ONLY conversion needed right now is from version -1/0 to version 1. Assume this is the case.

	// VERSION 0 TO VERSION 1:
	/*
	- Instances need to re-structure their info node to be info/local/FIELD
	- The platforms key is now stored under the info/local key as well, it is no longer a sibling to generation.
	- Instances converted from Version 0 to Version 1 are no longer considered or tagged as Legacy because Version 0 stuff tried to resolve item & model ID's upon addon consumption rather than on instance load.  Their info needs to be restructured still, but leave their resolved item & model ID's alone.
	- Library conversion should be done IMMEDIATELY upon any outdated library.db that gets loaded.
	- library.db files NEED to have their Library Version saved to their header, or be assumed Version 0.
	- Library Version should NOT change EVER, but ESPECIALLY after Redux is out of beta.
	- Conversion between Library Versions should be ABNORMAL behavior and only really needed by beta testers who don't want to lose their saves.
	*/

	// make sure the backups folder exists
	g_pFullFileSystem->CreateDirHierarchy("backups", "DEFAULT_WRITE_PATH");

	// find a suitable filename
	unsigned int backupNumber = 0;
	std::string backupFile = "backups/library-auto" + std::string(VarArgs("%u", backupNumber)) + ".db";

	while (g_pFullFileSystem->FileExists(backupFile.c_str(), "DEFAULT_WRITE_PATH"))
	{
		backupNumber++;
		backupFile = "backups/library-auto" + std::string(VarArgs("%u", backupNumber)) + ".db";
	}

	// copy the library.db file to it
	CUtlBuffer buf;
	if (g_pFullFileSystem->ReadFile("library.db", "DEFAULT_WRITE_PATH", buf))
		g_pFullFileSystem->WriteFile(backupFile.c_str(), "DEFAULT_WRITE_PATH", buf);
	buf.Purge();

	// Alright, a backup has been made.  Time to start converting.
	std::vector<std::string> badInstanceIds;
	std::vector<std::string> instanceIds;
	sqlite3* pDb = m_db;
	//unsigned int count = 0;

	sqlite3_stmt *stmtSelAll = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from instances", -1, &stmtSelAll, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	while (sqlite3_step(stmtSelAll) == SQLITE_ROW)
		instanceIds.push_back(std::string((const char*)sqlite3_column_text(stmtSelAll, 0)));
	sqlite3_finalize(stmtSelAll);

	for (unsigned int i = 0; i < instanceIds.size(); i++)
	{
		sqlite3_stmt *stmtSel = NULL;
		int rc = sqlite3_prepare(pDb, VarArgs("SELECT * from instances WHERE id = \"%s\"", instanceIds[i].c_str()), -1, &stmtSel, NULL);
		if (rc != SQLITE_OK)
		{
			DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));
			sqlite3_finalize(stmtSel);
			continue;
		}

		if (sqlite3_step(stmtSel) != SQLITE_ROW)
		{
			DevMsg("warning: did now find a row. skipping.\n");
			sqlite3_finalize(stmtSel);
			continue;
		}

		std::string rowId = std::string((const char*)sqlite3_column_text(stmtSel, 0));
//		DevMsg("Row ID is: %s\n", rowId.c_str());
		if (rowId == "")
		{
			sqlite3_finalize(stmtSel);
			continue;
		}

		int length = sqlite3_column_bytes(stmtSel, 1);
		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues detected as bad.\n");
			badInstanceIds.push_back(rowId);
			sqlite3_finalize(stmtSel);
			continue;
		}

		KeyValues* pInstance = new KeyValues("instance");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmtSel, 1), length);
		pInstance->ReadAsBinary(buf);

		// done with the select statement now
		sqlite3_finalize(stmtSel);
		buf.Purge();

		// instance is loaded & ready to convert
		KeyValues* oldInfoKV = pInstance->FindKey("info");
		if (!oldInfoKV)
		{
			// bogus save detected, clear it out... later
			badInstanceIds.push_back(rowId);
			pInstance->deleteThis();
			continue;
		}

		//KeyValues* infoKV = pInstance->FindKey("info/local", true);

		// copy subkeys from oldInfo to info
		for (KeyValues *sub = oldInfoKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		{
			if (sub->GetFirstSubKey())
				continue;	// don't copy some garbage values that show up.

			pInstance->SetString(VarArgs("info/local/%s", sub->GetName()), sub->GetString());
		}

		/*
		generation
		info (parent)
		title
		map
		style
		creator
		id
		platforms (parent) (sometimes)
		objects (parent) (sometimes)
		*/

		DevMsg("Converted instance w/ id %s\n", rowId.c_str());// and info : \n", rowId.c_str());
		//for (KeyValues *sub = pInstance->FindKey("info/local")->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		//	DevMsg("\t%s: %s\n", sub->GetName(), sub->GetString());

		//// Remove the platforms tab cuz it shouldn't be there.
		// Copy workshopId and mountId from the platform keys, if needed
		KeyValues* platformsKV = pInstance->FindKey(VarArgs("platforms/%s", AA_PLATFORM_ID));
		if (platformsKV)
		{
			for (KeyValues *sub = platformsKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
			{
				pInstance->SetString(VarArgs("info/local/platforms/%s/%s", AA_PLATFORM_ID, sub->GetName()), sub->GetString());
			}
		}

		std::vector<KeyValues*> targetSubKeys;
		// remove everything in the info key besides "local"
		for (KeyValues *sub = pInstance->FindKey("info")->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		{
			if (Q_strcmp(sub->GetName(), "local"))
				targetSubKeys.push_back(sub);
		}

		KeyValues* pOldInfoKV = pInstance->FindKey("info");
		for (unsigned int j = 0; j < targetSubKeys.size(); j++)
		{
			pOldInfoKV->RemoveSubKey(targetSubKeys[j]);
			//pInstance->SetString("info", "");
		}
		targetSubKeys.clear();

		// then remove the old platforms key
		if (pInstance->FindKey("platforms"))
		{
			pInstance->RemoveSubKey(pInstance->FindKey("platforms"));
			//pInstance->SetString("platforms", "");
			platformsKV = null;
		}

		// done removing stuff.

		//DevMsg("And now...\n");
		//for (KeyValues *sub = pInstance->FindKey("info/local")->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		//	DevMsg("\t%s: %s\n", sub->GetName(), sub->GetString());

		// Now save the instance back out to the library.db
		sqlite3_stmt *stmtInst = NULL;
		rc = sqlite3_prepare(pDb, VarArgs("REPLACE INTO instances VALUES(\"%s\", ?)", rowId.c_str()), -1, &stmtInst, NULL);
		if (rc != SQLITE_OK)
		{
			DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));
			sqlite3_finalize(stmtInst);
		}
		else
		{
			// SQLITE_STATIC because the statement is finalized before the buffer is freed:
			CUtlBuffer instBuf;
			pInstance->WriteAsBinary(instBuf);

			int size = instBuf.Size();
			rc = sqlite3_bind_blob(stmtInst, 1, instBuf.Base(), size, SQLITE_STATIC);
			if (rc != SQLITE_OK)
				DevMsg("FATAL ERROR: bind failed: %s\n", sqlite3_errmsg(pDb));
			else
			{
				rc = sqlite3_step(stmtInst);
				if (rc != SQLITE_DONE)
				{
					if (rc == SQLITE_ERROR)
						DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));
					else
						DevMsg("Weird other error!!\n");
				}
			}
			sqlite3_finalize(stmtInst);
			instBuf.Purge();
		}
		pInstance->deleteThis();
	}
	instanceIds.clear();

	// remove bad instances
	DevMsg("Removing %u bogus instances from old library.\n", badInstanceIds.size());
	for (unsigned int i = 0; i < badInstanceIds.size(); i++)
	{
		/*
		DELETE
		FROM
		artists_backup
		WHERE
		artistid = 1;
		*/

		sqlite3_stmt *stmtDel = NULL;
		rc = sqlite3_prepare(pDb, VarArgs("DELETE FROM instances WHERE id = \"%s\"", badInstanceIds[i].c_str()), -1, &stmtDel, NULL);
		if (rc != SQLITE_OK)
			DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));

		rc = sqlite3_step(stmtDel);
		if (rc != SQLITE_DONE)
			DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));

		sqlite3_finalize(stmtDel);
	}
	badInstanceIds.clear();

	//DevMsg("ERROR: Could not find old info key!  Contents include:\n");
	//for (KeyValues *sub = pInstance->GetFirstSubKey(); sub; sub = sub->GetNextKey())
	//	DevMsg("\t%s: %s\n", sub->GetName(), sub->GetString());

	// Now add the version table and set it to 1
	// now save our AA_LIBRARY_VERSION number to the DB for future proofing
	char *error;
	const char *sqlCreateVersionTable = "CREATE TABLE version (id INTEGER PRIMARY KEY, value INTEGER);";
	rc = sqlite3_exec(pDb, sqlCreateVersionTable, NULL, NULL, &error);
	if (rc != SQLITE_OK)
	{
		DevMsg("Error executing SQLite3 statement: %s\n", sqlite3_errmsg(pDb));
		sqlite3_free(error);
	}

	sqlite3_stmt *stmt3 = NULL;
	rc = sqlite3_prepare(pDb, "INSERT INTO version (id, value) VALUES(0, 1)", -1, &stmt3, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));

	rc = sqlite3_step(stmt3);
	if (rc != SQLITE_DONE)
		DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));

	sqlite3_finalize(stmt3);

	return true;
}

void C_MetaverseManager::Update()
{
	// FIXME: all this spawning object stuff should be done in instance manager, not here....
	if (m_pSpawningObjectEntity)
	{
		// Figure out where to place it
		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		Vector forward;
		pPlayer->EyeVectors(&forward);

		trace_t tr;
		UTIL_TraceLine(pPlayer->EyePosition(),
			pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH, MASK_NPCSOLID,
			pPlayer, COLLISION_GROUP_NONE, &tr);

		// No hit? We're done.
		if (tr.fraction == 1.0)
			return;

		VMatrix entToWorld;
		Vector xaxis;
		Vector yaxis;

		if (tr.plane.normal.z == 0.0f)
		{
			yaxis = Vector(0.0f, 0.0f, 1.0f);
			CrossProduct(yaxis, tr.plane.normal, xaxis);
			entToWorld.SetBasisVectors(tr.plane.normal, xaxis, yaxis);
		}
		else
		{
			Vector ItemToPlayer;
			VectorSubtract(pPlayer->GetAbsOrigin(), Vector(tr.endpos.x, tr.endpos.y, tr.endpos.z), ItemToPlayer);

			xaxis = Vector(ItemToPlayer.x, ItemToPlayer.y, ItemToPlayer.z);

			CrossProduct(tr.plane.normal, xaxis, yaxis);
			if (VectorNormalize(yaxis) < 1e-3)
			{
				xaxis.Init(0.0f, 0.0f, 1.0f);
				CrossProduct(tr.plane.normal, xaxis, yaxis);
				VectorNormalize(yaxis);
			}
			CrossProduct(yaxis, tr.plane.normal, xaxis);
			VectorNormalize(xaxis);

			entToWorld.SetBasisVectors(xaxis, yaxis, tr.plane.normal);
		}

		QAngle angles;
		MatrixToAngles(entToWorld, angles);

//		UTIL_SetOrigin(m_pSpawningObjectEntity, tr.endpos);
//		m_pSpawningObjectEntity->SetAbsAngles(angles);

		// WORKING!! Rotate based on how much player has turned since last frame
		/* disabled for now cuz of transform menu
		if (g_pAnarchyManager->GetMetaverseManager()->GetSpawningObjectEntity())
		{
			if (vgui::input()->IsKeyDown(KEY_E))
			{
				int x, y;
				vgui::input()->GetCursorPos(x, y);

				switch (m_iSpawningRotationAxis)
				{
					case 0:
						m_spawningAngles.x += x - ScreenWidth() / 2;
						break;

					case 1:
						m_spawningAngles.y += x - ScreenWidth() / 2;
						break;

					case 2:
						m_spawningAngles.z += x - ScreenWidth() / 2;
						break;
				}
			}

			angles.x += m_spawningAngles.x;
			angles.y += m_spawningAngles.y;
			angles.z += m_spawningAngles.z;
		}
		*/

		// add in the current transform
		transform_t* pTransform = g_pAnarchyManager->GetInstanceManager()->GetTransform();
		tr.endpos.x += pTransform->offX;
		tr.endpos.y += pTransform->offY;
		tr.endpos.z += pTransform->offZ;

		angles.x += pTransform->rotP;
		angles.y += pTransform->rotY;
		angles.z += pTransform->rotR;

		// FIXME: that stuff should be done server-side so collision boxes are updated properly...
		engine->ServerCmd(VarArgs("setcabpos %i %f %f %f %f %f %f \"%s\";\n", m_pSpawningObjectEntity->entindex(), tr.endpos.x, tr.endpos.y, tr.endpos.z, angles.x, angles.y, angles.z, ""), false);

		float currentScale = m_pSpawningObjectEntity->GetModelScale();
		if (abs(pTransform->scale - currentScale) > 0.04)
			g_pAnarchyManager->GetMetaverseManager()->SetObjectScale(g_pAnarchyManager->GetMetaverseManager()->GetSpawningObjectEntity(), pTransform->scale);
	}
}

//void C_MetaverseManager::ImportSteamGame(std::string name, std::string appid)
void C_MetaverseManager::ImportSteamGames(KeyValues* kv)
{
	// 1st save this out
	kv->SaveToFile(g_pFullFileSystem, "steamGames.key", "DEFAULT_WRITE_PATH");

	// now loop through it and add any missing games to the user library (but don't save them until the user makes a change to them.)
	std::string appid;
	std::string name;
	std::string detailsurl;
	std::string screenurl;
	std::string marqueeurl;

	for (KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey())
	{
		appid = sub->GetName();
		appid = appid.substr(2);
		name = sub->GetString();
		//DevMsg("Adding game %s w/ appid %s\n", name.c_str(), appid.c_str());

		// 1. try to find an item with this fileid
		// 2. if not found, create an item for it
		// 3. victory bowl

		// build the search info
		KeyValues* pSearchInfo = new KeyValues("search");	// this gets deleted by the metaverse manager!!
		pSearchInfo->SetString("file", appid.c_str());

		KeyValues* item = this->FindLibraryItem(pSearchInfo);
		if (!item)
		{
			//DevMsg("Adding Steam game w/ id %s - %s\n", appid.c_str(), name.c_str());
			item = new KeyValues("item");
			item->SetInt("generation", 3);
			item->SetInt("local/info/created", 0);
			item->SetString("local/info/owner", "local");
			item->SetInt("local/info/removed", 0);
			item->SetString("local/info/remover", "");
			item->SetString("local/info/alias", "");
			item->SetString("local/info/id", g_pAnarchyManager->GenerateUniqueId());
			item->SetString("local/title", name.c_str());
			item->SetString("local/description", "");
			item->SetString("local/file", appid.c_str());

			item->SetString("local/type", this->ResolveLegacyType("pc").c_str());
			item->SetString("local/app", this->ResolveLegacyApp("Steam").c_str());

			detailsurl = "http://store.steampowered.com/app/" + appid;
			item->SetString("local/reference", detailsurl.c_str());

			item->SetString("local/preview", "");
			item->SetString("local/download", "");
			item->SetString("local/stream", "");

			screenurl = "http://cdn.akamai.steamstatic.com/steam/apps/" + appid + "/page_bg_generated.jpg";	// not the best or most reliable url for a screenshot, but is a standard format.
			item->SetString("local/screen", screenurl.c_str());

			marqueeurl = "http://cdn.akamai.steamstatic.com/steam/apps/" + appid + "/header.jpg";
			item->SetString("local/marquee", marqueeurl.c_str());

			this->AddItem(item);

			// JUST SAVE THE ITEM OUT NOW.  this way steamGames.key doesn't have to be iterated through every time AArcade starts.
			g_pAnarchyManager->GetMetaverseManager()->SaveItem(item);
		}
		//else
		//	DevMsg("Already have a Steam game w/ id %s\n", appid.c_str());
	}

	kv->deleteThis();
}

void C_MetaverseManager::AddType(KeyValues* pType)
{
	KeyValues* active = this->GetActiveKeyValues(pType);
	if (active)
	{
		std::string id = VarArgs("%s", active->GetString("info/id"));
		m_types[id] = pType;
	}
}

void C_MetaverseManager::AddItem(KeyValues* pItem)
{
	KeyValues* active = this->GetActiveKeyValues(pItem);
	if (active)
	{
		std::string id = VarArgs("%s", active->GetString("info/id"));
		m_items[id] = pItem;
	}
}

void C_MetaverseManager::AddModel(KeyValues* pModel)
{
	KeyValues* active = this->GetActiveKeyValues(pModel);
	if (active)
	{
		std::string id = VarArgs("%s", active->GetString("info/id"));
		m_models[id] = pModel;
	}
}

void C_MetaverseManager::SaveSQL(sqlite3** pDb, const char* tableName, const char* id, KeyValues* kv)
{
	if (!pDb)
		pDb = &m_db;

	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(*pDb, VarArgs("REPLACE INTO %s VALUES(\"%s\", ?)", tableName, id), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(*pDb));

	// SQLITE_STATIC because the statement is finalized before the buffer is freed:
	CUtlBuffer buf;
	kv->WriteAsBinary(buf);

	int size = buf.Size();
	rc = sqlite3_bind_blob(stmt, 1, buf.Base(), size, SQLITE_STATIC);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: bind failed: %s\n", sqlite3_errmsg(*pDb));
	else
	{
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(*pDb));
	}
	sqlite3_finalize(stmt);
	buf.Purge();
}

void C_MetaverseManager::SaveItem(KeyValues* pItem, sqlite3* pDb)
{
	if (!pDb)
		pDb = m_db;

	KeyValues* active = this->GetActiveKeyValues(pItem);
	/*
	pItem->FindKey("current");
	if (!active)
		active = pItem->FindKey("local");
		*/

	// FIXME: ALWAYS SAVING TO ACTIVE, BUT WHEN OTHER KEY SLOTS GET USED, WILL HAVE TO SAVE TO USE LOGIC TO DETERMINE WHICH SUB-KEY WE'RE SAVING.
	// NOTE: We're only using the item to get the ID and to update its modified time.
	active->SetString("info/modified", VarArgs("%llu", g_pAnarchyManager->GetTimeNumber()));	// save as string because KeyValue's have no uint64 data type.

	// FIXME: SAVING TO .KEY FILES DISABLED FOR MYSQL MIGRATION!!
	// And dodged a bullet there, because when KV's get saved & loaded from disk, they forget their types, and ID's that start with 0 get screwed.
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, VarArgs("REPLACE INTO items VALUES(\"%s\", ?)", active->GetString("info/id")), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));

	// SQLITE_STATIC because the statement is finalized before the buffer is freed:
	CUtlBuffer buf;
	pItem->WriteAsBinary(buf);

	int size = buf.Size();
	rc = sqlite3_bind_blob(stmt, 1, buf.Base(), size, SQLITE_STATIC);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: bind failed: %s\n", sqlite3_errmsg(pDb));
	else
	{
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));
	}
	sqlite3_finalize(stmt);
	buf.Purge();
	//return rc;
	//pItem->SaveToFile(g_pFullFileSystem, VarArgs("library/items/%s.key", active->GetString("info/id")), "DEFAULT_WRITE_PATH");
	//DevMsg("Saved item %s to library/items/%s.key\n", active->GetString("title"), active->GetString("info/id"));
}

void C_MetaverseManager::SaveModel(KeyValues* pItem, sqlite3* pDb)
{
	if (!pDb)
		pDb = m_db;

	KeyValues* active = this->GetActiveKeyValues(pItem);
	/*
	pItem->FindKey("current");
	if (!active)
	active = pItem->FindKey("local");
	*/

	// FIXME: ALWAYS SAVING TO ACTIVE, BUT WHEN OTHER KEY SLOTS GET USED, WILL HAVE TO SAVE TO USE LOGIC TO DETERMINE WHICH SUB-KEY WE'RE SAVING.
	// NOTE: We're only using the item to get the ID and to update its modified time.
	active->SetString("info/modified", VarArgs("%llu", g_pAnarchyManager->GetTimeNumber()));	// save as string because KeyValue's have no uint64 data type.

	// FIXME: SAVING TO .KEY FILES DISABLED FOR MYSQL MIGRATION!!
	// And dodged a bullet there, because when KV's get saved & loaded from disk, they forget their types, and ID's that start with 0 get screwed.
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, VarArgs("REPLACE INTO models VALUES(\"%s\", ?)", active->GetString("info/id")), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));

	// SQLITE_STATIC because the statement is finalized before the buffer is freed:
	CUtlBuffer buf;
	pItem->WriteAsBinary(buf);

	int size = buf.Size();
	rc = sqlite3_bind_blob(stmt, 1, buf.Base(), size, SQLITE_STATIC);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: bind failed: %s\n", sqlite3_errmsg(pDb));
	else
	{
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));
	}
	sqlite3_finalize(stmt);
	buf.Purge();
}

void C_MetaverseManager::SaveType(KeyValues* pType, sqlite3* pDb)
{
	if (!pDb)
		pDb = m_db;

	KeyValues* active = this->GetActiveKeyValues(pType);

	// FIXME: ALWAYS SAVING TO ACTIVE, BUT WHEN OTHER KEY SLOTS GET USED, WILL HAVE TO SAVE TO USE LOGIC TO DETERMINE WHICH SUB-KEY WE'RE SAVING.
	// NOTE: We're only using the item to get the ID and to update its modified time.
	active->SetString("info/modified", VarArgs("%llu", g_pAnarchyManager->GetTimeNumber()));	// save as string because KeyValue's have no uint64 data type.

	// FIXME: SAVING TO .KEY FILES DISABLED FOR MYSQL MIGRATION!!
	// And dodged a bullet there, because when KV's get saved & loaded from disk, they forget their types, and ID's that start with 0 get screwed.
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, VarArgs("REPLACE INTO types VALUES(\"%s\", ?)", active->GetString("info/id")), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: prepare failed: %s\n", sqlite3_errmsg(pDb));

	// SQLITE_STATIC because the statement is finalized before the buffer is freed:
	CUtlBuffer buf;
	pType->WriteAsBinary(buf);

	int size = buf.Size();
	rc = sqlite3_bind_blob(stmt, 1, buf.Base(), size, SQLITE_STATIC);
	if (rc != SQLITE_OK)
		DevMsg("FATAL ERROR: bind failed: %s\n", sqlite3_errmsg(pDb));
	else
	{
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
			DevMsg("FATAL ERROR: execution failed: %s\n", sqlite3_errmsg(pDb));
	}
	sqlite3_finalize(stmt);
	buf.Purge();
}

// Create the item and return it, but don't save it or add it to the library.
bool C_MetaverseManager::CreateItem(int iLegacy, std::string itemId, KeyValues* pItemKV, std::string title, std::string description, std::string file, std::string type, std::string app, std::string reference, std::string preview, std::string download, std::string stream, std::string screen, std::string marquee, std::string model)
{
	// TODO NEXT TIME:
	// MAKE USE OF THIS CREATE ITEM METHOD EVERYWHERE POSSIBLE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (!pItemKV)
	{
		DevMsg("No item given to CreateItem! Aborting.\n");
		return false;
	}
	
	if (itemId == "")
		itemId = g_pAnarchyManager->GenerateUniqueId();

	pItemKV->SetInt("generation", 3);
	pItemKV->SetInt("legacy", iLegacy);

	KeyValues* pActiveKV = this->GetActiveKeyValues(pItemKV);

	// add standard info
	pActiveKV->SetString("info/id", itemId.c_str());
	pActiveKV->SetString("info/created", VarArgs("%llu", g_pAnarchyManager->GetTimeNumber()));
	pActiveKV->SetString("info/owner", "local");
	//pActiveKV->SetString("info/removed", "");
	//pActiveKV->SetString("info/remover", "");
	//pActiveKV->SetString("info/alias", "");

	pActiveKV->SetString("title", title.c_str());
	pActiveKV->SetString("description", description.c_str());
	pActiveKV->SetString("file", file.c_str());
	pActiveKV->SetString("type", type.c_str());	//AA_DEFAULT_TYPEID
	pActiveKV->SetString("app", app.c_str());
	pActiveKV->SetString("reference", reference.c_str());
	pActiveKV->SetString("preview", preview.c_str());
	pActiveKV->SetString("download", download.c_str());
	pActiveKV->SetString("stream", stream.c_str());
	pActiveKV->SetString("screen", screen.c_str());
	pActiveKV->SetString("marquee", marquee.c_str());
	pActiveKV->SetString("model", model.c_str());

	return pItemKV;
}

void C_MetaverseManager::SmartMergItemKVs(KeyValues* pItemA, KeyValues* pItemB, bool bPreferFirst)
{
	KeyValues* pKeeperItem = (bPreferFirst) ? pItemA : pItemB;
	KeyValues* pMergerItem = (bPreferFirst) ? pItemB : pItemA;

	KeyValues* pActiveKeeper = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pKeeperItem);
	KeyValues* pActiveMerger = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pMergerItem);

	if (Q_strcmp(pActiveKeeper->GetString("info/id"), pActiveMerger->GetString("info/id")))
		DevMsg("WARNING: SmargMerg-ing 2 items with different IDs: %s %s\n", pActiveKeeper->GetString("info/id"), pActiveMerger->GetString("info/id"));

	// pKeeperItem
	// - Contains the most user-customized versions of fields.
	// - Fields with local file paths are probably important.

	// Compare "app values.
	// If the Primary uses an app, keep it.  Otherwise, use the one from Secondary.
	if (!Q_strcmp(pActiveKeeper->GetString("app"), "") && Q_strcmp(pActiveMerger->GetString("app"), ""))
		pActiveKeeper->SetString("app", pActiveMerger->GetString("app"));

	// Compare "file" values.
	// If Primary uses a non-internet location, keep it.
	bool bSecondaryFileLocationIsGood = true;
	if (!Q_strcmp(pActiveMerger->GetString("file"), "") || !Q_strcmp(pActiveMerger->GetString("file"), pActiveMerger->GetString("title")) || !Q_stricmp(pActiveMerger->GetString("file"), pActiveKeeper->GetString("file")))
		bSecondaryFileLocationIsGood = false;

	if (bSecondaryFileLocationIsGood)
	{
		std::string primaryFileLocation = pActiveKeeper->GetString("file");

		bool bPrimaryFileLocationIsGood = true;
		if (!Q_strcmp(pActiveKeeper->GetString("file"), "") || !Q_strcmp(pActiveKeeper->GetString("file"), pActiveKeeper->GetString("title")) || primaryFileLocation.find("http://") == 0 || primaryFileLocation.find("https://") == 0 || primaryFileLocation.find("www.") == 0)
			bPrimaryFileLocationIsGood = false;

		if (!bPrimaryFileLocationIsGood)
		{
			bool bSecondaryFileLocationIsAWebImage = false;

			if (Q_strcmp(pActiveKeeper->GetString("file"), ""))
			{
				std::string imageTypes = "|jpg|jpeg|gif|png|bmp|tga|tbn|";

				// Find out if Secondary's "file" is a web image.
				std::string secondaryFileLocationExtension = pActiveMerger->GetString("file");
				if (secondaryFileLocationExtension.find("http://") == 0 || secondaryFileLocationExtension.find("https://") == 0 || secondaryFileLocationExtension.find("www.") == 0)
				{
					size_t found = secondaryFileLocationExtension.find_last_of(".");
					if (found != std::string::npos)
					{
						secondaryFileLocationExtension = secondaryFileLocationExtension.substr(found + 1);

						if (imageTypes.find(VarArgs("|%s|", secondaryFileLocationExtension.c_str())) != std::string::npos)
						{
							bSecondaryFileLocationIsAWebImage = true;
						}
					}
				}
			}

			if (bSecondaryFileLocationIsAWebImage)
			{
				// So, we have a "file" property about to be replaced with a web image.  We need to determine if we should replace the screen and marquee URLs now along with it.

				bool bPrimaryScreensLocationIsALocalImage = false;
				std::string primaryScreensLocation = pActiveKeeper->GetString("screen");

				if (primaryScreensLocation.find(":") == 1)
					bPrimaryScreensLocationIsALocalImage = true;

				if (!bPrimaryScreensLocationIsALocalImage)
				{
					if (!Q_strcmp(pActiveKeeper->GetString("screen"), "") || !Q_strcmp(pActiveKeeper->GetString("screen"), pActiveKeeper->GetString("file")))
					{
						pActiveKeeper->SetString("screen", pActiveMerger->GetString("file"));

						// Clear Primary's "screenslocation" as well so the image refreshes next time.
						// First,  delete the cached version of the image so it can be re-downloaded.
						/*
						if (Q_strcmp(pActiveKeeper->GetString("screen"), "") && g_pFullFileSystem->FileExists(pActiveKeeper->GetString("screen"), USE_GAME_PATH))
						{
							if (pClientArcadeResources->GetReallyDoneStarting())
							{
								pClientArcadeResources->DeleteLocalFile(pActiveKeeper->GetString("screenslocation"));
								pActiveKeeper->SetString("screenslocation", "");	// Only clear this if we are really gonna re-download it, for speed up 3/30/2015
							}
						}
						*/

						//						pPrimaryItemKV->SetString("screenslocation", "");	// Move this higher to try to improve speedup
					}
				}

				bool bPrimaryMarqueesLocationIsALocalImage = false;
				std::string primaryMarqueesLocation = pActiveKeeper->GetString("marquee");

				if (primaryMarqueesLocation.find(":") == 1)
					bPrimaryMarqueesLocationIsALocalImage = true;

				if (!bPrimaryMarqueesLocationIsALocalImage)
				{
					if (!Q_strcmp(pActiveKeeper->GetString("marquee"), "") || !Q_strcmp(pActiveKeeper->GetString("marquee"), pActiveKeeper->GetString("file")))
					{
						pActiveKeeper->SetString("marquee", pActiveMerger->GetString("file"));

						// Clear Primary's "marqueeslocation" as well so the image refreshes next time.
						// First,  delete the cached version of the image so it can be re-downloaded.
						/*
						if (Q_strcmp(pActiveKeeper->GetString("marqueeslocation"), "") && g_pFullFileSystem->FileExists(pActiveKeeper->GetString("marqueeslocation"), USE_GAME_PATH))
						{
							if (pClientArcadeResources->GetReallyDoneStarting())
							{
								pClientArcadeResources->DeleteLocalFile(pActiveKeeper->GetString("marqueeslocation"));

								pActiveKeeper->SetString("marqueeslocation", "");
							}
						}
						*/
					}
				}
			}

			// The "file" property is going to change, and if Primary has no "trailerurl", then Primary's "noembed" property should change along with it.
//			if (!Q_strcmp(pActiveKeeper->GetString("preview"), ""))
	//			pActiveKeeper->SetInt("noembed", pActiveMerger->GetInt("noembed"));


			// Now replace the "file" property.
			pActiveKeeper->SetString("file", pActiveMerger->GetString("file"));
		}
	}

	// Compare "screens/low" values.
	// If Primary's screenslocation is not a local image, use Secondary's.
	bool bSecondaryScreensURLIsGood = true;
	if (!Q_strcmp(pActiveMerger->GetString("screen"), "") || !Q_stricmp(pActiveMerger->GetString("screen"), pActiveKeeper->GetString("screen")))
		bSecondaryScreensURLIsGood = false;

	if (bSecondaryScreensURLIsGood)
	{
		bool bPrimaryScreensLocationIsALocalImage = false;
		std::string primaryScreensLocation = pActiveKeeper->GetString("screen");

		if (primaryScreensLocation.find(":") == 1)
			bPrimaryScreensLocationIsALocalImage = true;

		if (!bPrimaryScreensLocationIsALocalImage)
		{
			// So Secondary's "screens/low" is good, and there is no local image used on Primary's "screenslocation".
			pActiveKeeper->SetString("screen", pActiveMerger->GetString("screen"));

			// Clear Primary's "screenslocation" as well so the image refreshes next time.
			// First,  delete the cached version of the image so it can be re-downloaded.
			/*
			if (Q_strcmp(pActiveKeeper->GetString("screenslocation"), "") && g_pFullFileSystem->FileExists(pActiveKeeper->GetString("screenslocation"), USE_GAME_PATH))
			{
				if (pClientArcadeResources->GetReallyDoneStarting())
				{
					pClientArcadeResources->DeleteLocalFile(pActiveKeeper->GetString("screenslocation"));
					pActiveKeeper->SetString("screenslocation", "");
				}
			}
			*/
		}
	}

	// Compare "marquees/low" values.
	// If Primary's marqueeslocation is not a local image, use Secondary's.
	bool bSecondaryMarqueesURLIsGood = true;
	if (!Q_strcmp(pActiveMerger->GetString("marquee"), "") || !Q_stricmp(pActiveMerger->GetString("marquee"), pActiveKeeper->GetString("marquee")))
		bSecondaryMarqueesURLIsGood = false;

	if (bSecondaryMarqueesURLIsGood)
	{
		bool bPrimaryMarqueesLocationIsALocalImage = false;
		std::string primaryMarqueesLocation = pActiveKeeper->GetString("marquee");

		if (primaryMarqueesLocation.find(":") == 1)
			bPrimaryMarqueesLocationIsALocalImage = true;

		if (!bPrimaryMarqueesLocationIsALocalImage)
		{
			// So Secondary's "marquees/low" is good, and there is no local image used on Primary's "marqueeslocation".
			pActiveKeeper->SetString("marquee", pActiveMerger->GetString("marquee"));

			// Clear Primary's "marqueeslocation" as well so the image refreshes next time.
			// First,  delete the cached version of the image so it can be re-downloaded.
			/*
			if (Q_strcmp(pActiveKeeper->GetString("marqueeslocation"), "") && g_pFullFileSystem->FileExists(pActiveKeeper->GetString("marqueeslocation"), USE_GAME_PATH))
			{
				if (pClientArcadeResources->GetReallyDoneStarting())
				{
					pClientArcadeResources->DeleteLocalFile(pActiveKeeper->GetString("marqueeslocation"));
					pActiveKeeper->SetString("marqueeslocation", "");
				}
			}
			*/
		}
	}

	// Compare "trailerurl" values
	if (Q_strcmp(pActiveMerger->GetString("preview"), ""))
		pActiveKeeper->SetString("preview", pActiveMerger->GetString("preview"));

	/*
	// Compare "publishedfileid" values
	if (Q_strcmp(pActiveMerger->GetString("publishedfileid"), ""))
		pActiveKeeper->SetString("publishedfileid", pActiveMerger->GetString("publishedfileid"));
	*/

	// Compare "model" values
	if (Q_strcmp(pActiveMerger->GetString("model"), ""))
		pActiveKeeper->SetString("model", pActiveMerger->GetString("model"));

	// Compare "type" values
	// Always use Secondary's "type" (unless maybe we have a m_pKeepMyItem"Names"ConVar?)
	pActiveKeeper->SetString("type", pActiveMerger->GetString("type"));

	// If there was a m_bKeepMyItemNamesConvar, this is where it should be used.
	/*
	if (bFullMerg)
	{
		// Compare "title" values
		if (Q_strcmp(pActiveMerger->GetString("title"), ""))
			pActiveKeeper->SetString("title", pActiveMerger->GetString("title"));

		// Compare "description" values
		if (Q_strcmp(pActiveMerger->GetString("description"), ""))
			pActiveKeeper->SetString("description", pActiveMerger->GetString("description"));

		// Compare "downloadurl" values
		if (Q_strcmp(pActiveMerger->GetString("downloadurl"), ""))
			pActiveKeeper->SetString("downloadurl", pActiveMerger->GetString("downloadurl"));

		// Compare "detailsurl" values
		if (Q_strcmp(pActiveMerger->GetString("detailsurl"), ""))
			pActiveKeeper->SetString("detailsurl", pActiveMerger->GetString("detailsurl"));

		// Compare "comments" values
		if (Q_strcmp(pActiveMerger->GetString("comments"), ""))
			pActiveKeeper->SetString("comments", pActiveMerger->GetString("comments"));

		// Compare "instructions" values
		if (Q_strcmp(pActiveMerger->GetString("instructions"), ""))
			pActiveKeeper->SetString("instructions", pActiveMerger->GetString("instructions"));

		// Compare "group" values
		if (Q_strcmp(pActiveMerger->GetString("group"), ""))
			pActiveKeeper->SetString("group", pActiveMerger->GetString("group"));

		// Compare "groupurl" values
		if (Q_strcmp(pActiveMerger->GetString("groupurl"), ""))
			pActiveKeeper->SetString("groupurl", pActiveMerger->GetString("groupurl"));

		// Compare "addon" values
		if (Q_strcmp(pActiveMerger->GetString("addon"), ""))
			pActiveKeeper->SetString("addon", pActiveMerger->GetString("addon"));
	}
	*/
}

std::vector<std::string>* C_MetaverseManager::GetDefaultFields()
{
	return &m_defaultFields;
}

void C_MetaverseManager::UpdateScrapersJS()
{
	std::vector<std::string> scraperList;

	// get a list of all the .js files in the folder
	FileFindHandle_t handle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx("resource\\ui\\html\\scrapers\\*.js", "MOD", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		scraperList.push_back(pFilename);
		pFilename = g_pFullFileSystem->FindNext(handle);
	}

	// generate code
	std::string code = "";
	unsigned int max = scraperList.size();
	for (unsigned int i = 0; i < max; i++)
		code += "arcadeHud.loadHeadScript(\"scrapers/" + scraperList[i] + "\");\n";

	// open up scrapers.js and replace it.  this path is greated when AArcade is started.
	FileHandle_t fh = filesystem->Open("resource\\ui\\html\\scrapers.js", "w", "DEFAULT_WRITE_PATH");
	if (fh)
	{
		//filesystem->FPrintf(fh, "%s", code.c_str());
		filesystem->Write(code.c_str(), V_strlen(code.c_str()), fh);
		filesystem->Close(fh);
	}
}

bool C_MetaverseManager::LoadSQLKevValues(const char* tableName, const char* id, KeyValues* kv, sqlite3* pDb)
{
	if (!pDb)
		pDb = m_db;

	sqlite3_stmt *stmt = NULL;
	//DevMsg("loading from table name: %s _id %s\n", tableName, id);
	int rc = sqlite3_prepare(pDb, VarArgs("SELECT * from %s WHERE id = \"%s\"", tableName, id), -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	bool bSuccess = false;
	int length;
	if (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		if (length > 0)
		{
			CUtlBuffer buf(0, length, 0);
			buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
			if (kv->ReadAsBinary(buf))
				bSuccess = true;
			buf.Purge();
		}
		else
			bSuccess = false;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.

	return bSuccess;
}

KeyValues* C_MetaverseManager::LoadLocalItemLegacy(bool& bIsModel, bool& bWasAlreadyLoaded, std::string file, std::string filePath, std::string workshopIds, std::string mountIds, C_Backpack* pBackpack, std::string searchPath, bool bShouldAddToActiveLibrary)
{
	KeyValues* pItem = new KeyValues("item");
	bool bLoaded;
	
	bool bWasLoaded = false;

	std::string fullFile = filePath + file;
	if (searchPath != "")
	{
		bLoaded = pItem->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), searchPath.c_str());
	}
	else if (filePath != "")
		bLoaded = pItem->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), "");
	else
		bLoaded = pItem->LoadFromFile(g_pFullFileSystem, file.c_str(), "MOD");

	bool bResponseIsModel;
	if ( !bLoaded )
	{
		pItem->deleteThis();
		pItem = null;
		bResponseIsModel = false;
	}
	else
	{
		// flag us as being loaded from legacy
		pItem->SetBool("loadedFromLegacy", true);

		// determine the generation of this item
		KeyValues* pGeneration = pItem->FindKey("generation");
		if (!pGeneration)
		{
			// update us to 3rd generation
			pItem->SetInt("generation", 3);

			// add standard info (except for id)
			//pItem->SetInt("local/info/created", 0);
			pItem->SetString("local/info/owner", "local");
			//pItem->SetInt("local/info/removed", 0);
			//pItem->SetString("local/info/remover", "");
			//pItem->SetString("local/info/alias", "");

			// determine if this is a model or not
			std::string modelFile = pItem->GetString("filelocation");
			//if (modelFile.find("cabinets") >= 0)
				//DevMsg("Harrrr: %s\n", modelFile.c_str());
			size_t foundExt = modelFile.find(".mdl");
			if (foundExt == modelFile.length() - 4)
			{
				bResponseIsModel = true;
				std::string itemId = g_pAnarchyManager->GenerateLegacyHash(pItem->GetString("filelocation"));
				
				pItem->SetString("local/info/id", itemId.c_str());
				pItem->SetString("local/title", pItem->GetString("title"));
				pItem->SetString("local/preview", pItem->GetString("trailerurl"));

				if (Q_strcmp(pItem->GetString("group"), ""))
					pItem->SetString("local/keywords", pItem->GetString("group"));
				//pItem->SetInt("local/dynamic", 0);
				pItem->SetString(VarArgs("local/platforms/%s/id", AA_PLATFORM_ID), AA_PLATFORM_ID);
				pItem->SetString(VarArgs("local/platforms/%s/file", AA_PLATFORM_ID), modelFile.c_str());
				pItem->SetString(VarArgs("local/platforms/%s/download", AA_PLATFORM_ID), "");

				pItem->SetString(VarArgs("local/platforms/%s/workshopId", AA_PLATFORM_ID), workshopIds.c_str());
				pItem->SetString(VarArgs("local/platforms/%s/mountId", AA_PLATFORM_ID), mountIds.c_str());
//				pItem->SetString(VarArgs("local/platforms/%s/backpackId", AA_PLATFORM_ID), backpackId.c_str());

				// models can be loaded right away because they don't depend on anything else, like items do. (items depend on models)
				//DevMsg("Loading model with ID %s and model %s\n", itemId.c_str(), modelFile.c_str());

				auto it = m_models.find(itemId);
				if (it != m_models.end())
				{
					pItem->deleteThis();
					pItem = it->second;
					bWasLoaded = true;
				}
				else if (bShouldAddToActiveLibrary)
					m_models[itemId] = pItem;
			}
			else
			{
				bResponseIsModel = false;

				std::string itemId = g_pAnarchyManager->ExtractLegacyId(file, pItem);
				pItem->SetString("local/info/id", itemId.c_str());
				pItem->SetString(VarArgs("local/platforms/%s/workshopId", AA_PLATFORM_ID), workshopIds.c_str());
				pItem->SetString(VarArgs("local/platforms/%s/mountId", AA_PLATFORM_ID), mountIds.c_str());
				//pItem->SetString(VarArgs("local/platforms/%s/backpackId", AA_PLATFORM_ID), backpackId.c_str());

				pItem->SetString("local/title", pItem->GetString("title"));

				if (Q_strcmp(pItem->GetString("group"), ""))
					pItem->SetString("local/keywords", pItem->GetString("group"));

				pItem->SetString("local/description", pItem->GetString("description"));
				pItem->SetString("local/file", pItem->GetString("filelocation"));

				// NEEDS RESOLVING!!
				std::string legacyType = pItem->GetString("type");
				std::string resolvedType = this->ResolveLegacyType(legacyType);
				pItem->SetString("local/type", resolvedType.c_str());

				// NEEDS RESOLVING!!
				std::string legacyApp = pItem->GetString("app");
				std::string resolvedApp = this->ResolveLegacyApp(legacyApp);
				pItem->SetString("local/app", resolvedApp.c_str());
				pItem->SetString("local/reference", pItem->GetString("detailsurl"));
				pItem->SetString("local/preview", pItem->GetString("trailerurl"));
				pItem->SetString("local/download", pItem->GetString("downloadurl"));

				// NEEDS RESOLVING!!
				std::string resolvedScreen = pItem->GetString("screens/low");
				if (resolvedScreen == "")
				{
					resolvedScreen = pItem->GetString("screenslocation");
					if (resolvedScreen.find(":") != 1 || !g_pFullFileSystem->FileExists(resolvedScreen.c_str(), ""))
						resolvedScreen = "";
				}
				pItem->SetString("local/screen", resolvedScreen.c_str());


				/*
				// CACHE NEEDS RESOLVING!!
				//std::string legacyPath = "A:\\SteamLibrary\\steamapps\\common\\Anarchy Arcade\\aarcade\\";
				char fullPathBuf[AA_MAX_STRING];
				std::string resolvedScreenCached = pItem->GetString("screenslocation");
				//std::string resolvedScreenCached = VarArgs("%s\\%s\\screens\\%s.tbn", library_type.c_str(), newType.c_str(), this->GenerateHashX(itemfile_ref.c_str();
				//BuildItemKV->SetString("marqueeslocation", UTIL_VarArgs("%s\\%s\\marquees\\%s.tbn", library_type.c_str(), newType.c_str(), this->GenerateHashX(itemfile_ref.c_str())));
				//BuildItemKV->SetString("screenslocation", UTIL_VarArgs("%s\\%s\\screens\\%s.tbn", library_type.c_str(), newType.c_str(), this->GenerateHashX(itemfile_ref.c_str())));

				if (resolvedScreenCached != "" && resolvedScreenCached.find(":") != 1)
				{
					g_pFullFileSystem->RelativePathToFullPath(resolvedScreenCached.c_str(), "MOD", fullPathBuf, AA_MAX_STRING);
					resolvedScreenCached = fullPathBuf;

					if (resolvedScreenCached.find(":") != 1)
					{
						DevMsg("Testing sample path: %s\n", file.c_str());
						resolvedScreenCached = VarArgs("%s\\%s\\screens\\%s.tbn", "library", pItem->GetString("type"), g_pAnarchyManager->GenerateLegacyHash(file.c_str()));
						g_pFullFileSystem->RelativePathToFullPath(resolvedScreenCached.c_str(), "MOD", fullPathBuf, AA_MAX_STRING);
						resolvedScreenCached = fullPathBuf;

						if (resolvedScreenCached.find(":") != 1)
						{
							DevMsg("Testing sample path: %s\n", file.c_str());
							resolvedScreenCached = VarArgs("%s\\%s\\screens\\%s.tbn", "library_cache", pItem->GetString("type"), g_pAnarchyManager->GenerateLegacyHash(file.c_str()));
							g_pFullFileSystem->RelativePathToFullPath(resolvedScreenCached.c_str(), "MOD", fullPathBuf, AA_MAX_STRING);
							resolvedScreenCached = fullPathBuf;

							if (resolvedScreenCached.find(":") != 1)
								resolvedScreenCached = "";
						}
					}

					DevMsg("Arbys full path here is: %s\n", resolvedScreenCached.c_str());
				}
				else
					resolvedScreenCached = "";
				pItem->SetString("local/screencached", resolvedScreenCached.c_str());
				*/



				// NEEDS RESOLVING!!
				std::string resolvedMarquee = pItem->GetString("marquees/low");
				if (resolvedMarquee == "")
				{
					resolvedMarquee = pItem->GetString("marqueeslocation");
					if (resolvedMarquee.find(":") != 1 || !g_pFullFileSystem->FileExists(resolvedMarquee.c_str(), ""))
						resolvedMarquee = "";
				}
				pItem->SetString("local/marquee", resolvedMarquee.c_str());



				/*
				// CACHE NEEDS RESOLVING!!
				//std::string legacyPath = "A:\\SteamLibrary\\steamapps\\common\\Anarchy Arcade\\aarcade\\";
				//char fullPathBuf[AA_MAX_STRING];
				std::string resolvedMarqueeCached = pItem->GetString("marqueeslocation");
				if (resolvedMarqueeCached != "" && resolvedMarqueeCached.find(":") != 1)
				{
					g_pFullFileSystem->RelativePathToFullPath(resolvedMarqueeCached.c_str(), "MOD", fullPathBuf, AA_MAX_STRING);
					resolvedMarqueeCached = fullPathBuf;
					DevMsg("Arbys full path here is: %s\n", resolvedMarqueeCached.c_str());
				}
				else
					resolvedMarqueeCached = "";
				pItem->SetString("local/marqueecached", resolvedMarqueeCached.c_str());
				*/

				/*
				auto it = m_items.find(itemId);
				if (it != m_items.end())
				{
					pItem->deleteThis();
					pItem = it->second;
					bWasLoaded = true;
				}
				else if (bShouldAddToActiveLibrary)
					m_items[itemId] = pItem;
				*/

				if ( bShouldAddToActiveLibrary)
					m_previousLoadLocalItemsLegacyBuffer.push_back(pItem);
			}
		}
		else
			bResponseIsModel = false;
	}

	bIsModel = bResponseIsModel;
	bWasAlreadyLoaded = bWasLoaded;
	return pItem;
}

void C_MetaverseManager::LoadFirstLocalItemLegacy(bool bFastMode, std::string filePath, std::string workshopIds, std::string mountIds, C_Backpack* pBackpack)
{
	// only need to do this on the first one.
	//bool bBadFastMode = true;

	if (m_previousLoadLocalItemLegacyFilePath != "")
		this->LoadLocalItemLegacyClose();

	// start it
	m_previousLoadLocalItemLegacyFastMode = bFastMode;
	m_previousLoadLocalItemLegacyFilePath = filePath;
	m_previousLocaLocalItemLegacyWorkshopIds = workshopIds;
	m_previousLoadLocalItemLegacyMountIds = mountIds;
	m_pPreviousLoadLocalItemLegacyBackpack = pBackpack;

	unsigned int uMountWorkshopNumModels = 0;
	unsigned int uMountWorkshopNumItems = 0;

	C_AwesomiumBrowserInstance* pHudBrowserInstance = g_pAnarchyManager->GetAwesomiumBrowserManager()->FindAwesomiumBrowserInstance("hud");

	KeyValues* pItem;
	std::string fullPath = m_previousLoadLocalItemLegacyFilePath + "library\\*";
	const char *pFoldername = g_pFullFileSystem->FindFirst(fullPath.c_str(), &m_previousLoadLocalItemLegacyFolderHandle);
	while (pFoldername != NULL)
	{
		if (!Q_strcmp(pFoldername, ".") || !Q_strcmp(pFoldername, ".."))
		{
			pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
			continue;
		}

		m_previousLoadLocalItemLegacyFolderPath = VarArgs("library\\%s", pFoldername);
		if (!g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalItemLegacyFolderHandle))
		{
			DevMsg("All items files must be within a subfolder!\n");
			pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
			continue;
		}

		const char *pFilename = g_pFullFileSystem->FindFirst(VarArgs("%s%s\\*.itm", m_previousLoadLocalItemLegacyFilePath.c_str(), m_previousLoadLocalItemLegacyFolderPath.c_str()), &m_previousLoadLocalItemLegacyFileHandle);
		while (pFilename != NULL)
		{
			if (g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalItemLegacyFileHandle))
			{
				pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
				continue;
			}

			// WE'VE FOUND A FILE TO ATTEMPT TO LOAD!!!
			std::string foundName = VarArgs("%s\\%s", m_previousLoadLocalItemLegacyFolderPath.c_str(), pFilename);
			//pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);

			// MAKE THE FILE PATH NICE
			char path_buffer[AA_MAX_STRING];
			Q_strcpy(path_buffer, foundName.c_str());
			V_FixSlashes(path_buffer);
			foundName = path_buffer;
			// FINISHED MAKING THE FILE PATH NICE

			bool bIsModel;
			bool bWasAlreadyLoaded;
			pItem = this->LoadLocalItemLegacy(bIsModel, bWasAlreadyLoaded, foundName, m_previousLoadLocalItemLegacyFilePath, m_previousLocaLocalItemLegacyWorkshopIds, m_previousLoadLocalItemLegacyMountIds, m_pPreviousLoadLocalItemLegacyBackpack);
			if (pItem)
			{
				if (bIsModel)
					uMountWorkshopNumModels++;
				else
					uMountWorkshopNumItems++;

				if (!bFastMode)
				{
					if (bIsModel)
					{
						if (m_previousLocaLocalItemLegacyWorkshopIds != "")
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Models", "workshoplibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
						else
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Models", "oldlibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
					}
					else
					{
						if (m_previousLocaLocalItemLegacyWorkshopIds != "")
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Items", "workshoplibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
						else
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Items", "oldlibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
					}
					return;
				}
			}

			pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
		}

		g_pFullFileSystem->FindClose(m_previousLoadLocalItemLegacyFileHandle);
		pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
		//break;
	}

	if (bFastMode)
	{
		if (m_previousLocaLocalItemLegacyWorkshopIds != "")
		{
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Models", "workshoplibrarymodels", "", "", std::string(VarArgs("+%u", uMountWorkshopNumModels)));
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Items", "workshoplibraryitems", "", "", std::string(VarArgs("+%u", uMountWorkshopNumItems)));
		}
		else
		{
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Models", "oldlibrarymodels", "", "", std::string(VarArgs("+%u", uMountWorkshopNumModels)));
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Items", "oldlibraryitems", "", "", std::string(VarArgs("+%u", uMountWorkshopNumItems)));
		}
	}

	g_pAnarchyManager->GetWorkshopManager()->OnMountWorkshopSucceed();	
	this->LoadLocalItemLegacyClose();
	return;
}

void C_MetaverseManager::LoadNextLocalItemLegacy()
{
	C_AwesomiumBrowserInstance* pHudBrowserInstance = g_pAnarchyManager->GetAwesomiumBrowserManager()->FindAwesomiumBrowserInstance("hud");

	unsigned int uMountWorkshopNumModels = 0;
	unsigned int uMountWorkshopNumItems = 0;

	KeyValues* pItem;
	const char *pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalItemLegacyFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
			continue;
		}

		// WE'VE FOUND A FILE TO ATTEMPT TO LOAD!!!
		std::string foundName = VarArgs("%s\\%s", m_previousLoadLocalItemLegacyFolderPath.c_str(), pFilename);
		//pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);

		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);
		foundName = path_buffer;
		// FINISHED MAKING THE FILE PATH NICE

		bool bIsModel;
		bool bWasAlreadyLoaded;
		pItem = this->LoadLocalItemLegacy(bIsModel, bWasAlreadyLoaded, foundName, m_previousLoadLocalItemLegacyFilePath, m_previousLocaLocalItemLegacyWorkshopIds, m_previousLoadLocalItemLegacyMountIds, m_pPreviousLoadLocalItemLegacyBackpack);
		if (pItem)
		{
			if (bIsModel)
				uMountWorkshopNumModels++;
			else
				uMountWorkshopNumItems++;

			if (!m_previousLoadLocalItemLegacyFastMode)
			{
				if (bIsModel)
				{
					if (m_previousLocaLocalItemLegacyWorkshopIds != "")
						pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Models", "workshoplibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
					else
						pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Models", "oldlibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
				}
				else
				{
					if (m_previousLocaLocalItemLegacyWorkshopIds != "")
						pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Items", "workshoplibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
					else
						pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Items", "oldlibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
				}
				return;
			}
		}

		pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
		//break;
	}

	g_pFullFileSystem->FindClose(m_previousLoadLocalItemLegacyFileHandle);

	// done searching a folder, continue on to next folder
	std::string fullPath = m_previousLoadLocalItemLegacyFilePath + "library\\*";
	const char *pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
	while (pFoldername != NULL)
	{
		if (!Q_strcmp(pFoldername, ".") || !Q_strcmp(pFoldername, ".."))
		{
			pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
			continue;
		}

		m_previousLoadLocalItemLegacyFolderPath = VarArgs("library\\%s", pFoldername);
		if (!g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalItemLegacyFolderHandle))
		{
			DevMsg("All items files must be within a subfolder!\n");
			pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
			continue;
		}

		const char *pFilename = g_pFullFileSystem->FindFirst(VarArgs("%s%s\\*.itm", m_previousLoadLocalItemLegacyFilePath.c_str(), m_previousLoadLocalItemLegacyFolderPath.c_str()), &m_previousLoadLocalItemLegacyFileHandle);
		while (pFilename != NULL)
		{
			if (g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalItemLegacyFileHandle))
			{
				pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
				continue;
			}

			// WE'VE FOUND A FILE TO ATTEMPT TO LOAD!!!
			std::string foundName = VarArgs("%s\\%s", m_previousLoadLocalItemLegacyFolderPath.c_str(), pFilename);

			// MAKE THE FILE PATH NICE
			char path_buffer[AA_MAX_STRING];
			Q_strcpy(path_buffer, foundName.c_str());
			V_FixSlashes(path_buffer);
			foundName = path_buffer;
			// FINISHED MAKING THE FILE PATH NICE

			bool bIsModel;
			bool bWasAlreadyLoaded;
			pItem = this->LoadLocalItemLegacy(bIsModel, bWasAlreadyLoaded, foundName, m_previousLoadLocalItemLegacyFilePath, m_previousLocaLocalItemLegacyWorkshopIds, m_previousLoadLocalItemLegacyMountIds, m_pPreviousLoadLocalItemLegacyBackpack);
			if (pItem)
			{
				if (bIsModel)
					uMountWorkshopNumModels++;
				else
					uMountWorkshopNumItems++;

				if (!m_previousLoadLocalItemLegacyFastMode)
				{
					if (bIsModel)
					{
						if (m_previousLocaLocalItemLegacyWorkshopIds != "")
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Models", "workshoplibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
						else
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Models", "oldlibrarymodels", "", "", "+", "loadNextLocalItemLegacyCallback");
					}
					else
					{
						if (m_previousLocaLocalItemLegacyWorkshopIds != "")
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Items", "workshoplibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
						else
							pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Items", "oldlibraryitems", "", "", "+", "loadNextLocalItemLegacyCallback");
					}
					return;
				}
			}

			pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFileHandle);
		}

		g_pFullFileSystem->FindClose(m_previousLoadLocalItemLegacyFileHandle);
		pFoldername = g_pFullFileSystem->FindNext(m_previousLoadLocalItemLegacyFolderHandle);
		//break;
	}

	if (m_previousLoadLocalItemLegacyFastMode)
	{
		if (m_previousLocaLocalItemLegacyWorkshopIds != "")
		{
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Models", "workshoplibrarymodels", "", "", std::string(VarArgs("+%u", uMountWorkshopNumModels)));
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Loading Workshop Items", "workshoplibraryitems", "", "", std::string(VarArgs("+%u", uMountWorkshopNumItems)));
		}
		else
		{
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Models", "oldlibrarymodels", "", "", std::string(VarArgs("+%u", uMountWorkshopNumModels)));
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Importing Old AArcade Items", "oldlibraryitems", "", "", std::string(VarArgs("+%u", uMountWorkshopNumItems)));
		}
	}

	g_pAnarchyManager->GetWorkshopManager()->OnMountWorkshopSucceed();
	//g_pAnarchyManager->GetMetaverseManager()->OnMountWorkshopSucceed();
	this->LoadLocalItemLegacyClose();
	return;
}

void C_MetaverseManager::LoadLocalItemLegacyClose()
{
	if (m_previousLoadLocalItemLegacyFilePath != "")
	{
//		g_pFullFileSystem->FindClose(m_previousLoadLocalItemLegacyFolderHandle);

//		if (m_previousLoadLocalItemLegacyFolderPath != "")
//			g_pFullFileSystem->FindClose(m_previousLoadLocalItemLegacyFileHandle);
		m_previousLoadLocalItemLegacyFastMode = false;
		m_previousLoadLocalItemLegacyFilePath = "";
		m_previousLoadLocalItemLegacyFolderPath = "";
		m_previousLocaLocalItemLegacyWorkshopIds = "";
		m_previousLoadLocalItemLegacyMountIds = "";
		m_pPreviousLoadLocalItemLegacyBackpack = null;
//		m_previousLoadLocalItemsLegacyBuffer.clear();
	}
}

void C_MetaverseManager::ResolveLoadLocalItemLegacyBuffer()
{
	// FIXME This is a huge bottleneck

	// This usually gets done after all  workshop mounts are done, but since this is an after-the-fact one, gotta do it again manually here
	std::string legacyModelId;
	unsigned int numResponses = m_previousLoadLocalItemsLegacyBuffer.size();
	unsigned int i;
	unsigned int j;
	KeyValues* pCompoundItemKV;
	KeyValues* active;
	unsigned int numVictims;
	std::vector<KeyValues*> victims;
	//KeyValues* model;
	//KeyValues* modelActive;
	for (i = 0; i < numResponses; i++)
	{
		pCompoundItemKV = m_previousLoadLocalItemsLegacyBuffer[i];

		// OSOLETE! Legacy models & items are no longer pre-resolved like this!
		//active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pCompoundItemKV);
		//std::string resolvedModel = g_pAnarchyManager->GenerateLegacyHash(pCompoundItemKV->GetString("lastmodel"));
		//active->SetString("model", resolvedModel.c_str());

		// legacy model (gets resolved upon map load)
		legacyModelId = pCompoundItemKV->GetString("lastmodel");

		// remove everything not in local or current or generation
		for (KeyValues *sub = pCompoundItemKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		{
			if (Q_strcmp(sub->GetName(), "local") && Q_strcmp(sub->GetName(), "generation"))
				victims.push_back(sub);
		}

		numVictims = victims.size();
		for (j = 0; j < numVictims; j++)
			pCompoundItemKV->RemoveSubKey(victims[j]);

		if (numVictims > 0)
			victims.clear();

		// the itemKV is no longer compound!  It has been stripped down to a GEN3 item now!
		KeyValues* pItemKV = pCompoundItemKV;
		KeyValues* pActiveKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pItemKV);

		//for (KeyValues *sub = pActiveKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		//	DevMsg("Key & Value: %s & %s\n", sub->GetName(), sub->GetString());

		std::string id = VarArgs("%s", pActiveKV->GetString("info/id"));

		KeyValues* pTestItemKV;
		KeyValues* pTestActiveKV;
		auto it = m_items.find(id);
		if (it != m_items.end())
		{
			pTestItemKV = it->second;
			pTestActiveKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pTestItemKV);

			// this is a legacy item, so give the right-of-way to any generation 3 item that was already found.
			if (pTestItemKV->GetInt("legacy") == 1)
			{
				// merg this legacy item with the other legacy item
				this->SmartMergItemKVs(pTestItemKV, pItemKV);
				pItemKV->deleteThis();
				pItemKV = it->second;

				//// FIXME: For now, just delete the old one and let this one overpower it.
				//it->second->deleteThis();
				//m_items.erase(it);
				//m_items[id] = pItem;
			}
			else
			{
				// let the generation 3 item overpower us
				pItemKV->deleteThis();
				pItemKV = it->second;
			}
		}
		else
			m_items[id] = pItemKV;
	}

	m_previousLoadLocalItemsLegacyBuffer.clear();
}

KeyValues* C_MetaverseManager::LoadLocalType(std::string file, std::string filePath)
{
	// TODO: IS THIS OBSOLETE?
	KeyValues* pType = new KeyValues("type");
	bool bLoaded;

	if (filePath != "")
	{
		std::string fullFile = filePath + file;
		bLoaded = pType->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), "");
	}
	else
		bLoaded = pType->LoadFromFile(g_pFullFileSystem, file.c_str(), "MOD");

	if (!bLoaded)
	{
		pType->deleteThis();
		pType = null;
	}
	else
	{
		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pType);
		std::string id = pActive->GetString("info/id");
		m_types[id] = pType;
	}

	return pType;
}

unsigned int C_MetaverseManager::LoadAllLocalTypes(sqlite3* pDb, std::map<std::string, KeyValues*>* pResponseMap)
{
	if (!pDb)
		pDb = m_db;
	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from types", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	int length;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
			continue;
		}

		KeyValues* pType = new KeyValues("type");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
		pType->ReadAsBinary(buf);
		buf.Purge();

		// TODO: Look up any alias here first!!
		KeyValues* pActive = this->GetActiveKeyValues(pType);
		std::string id = pActive->GetString("info/id");
		if (pResponseMap)
			(*pResponseMap)[id] = pType;
		else
			m_types[id] = pType;
		
		DevMsg("Loaded type %s from Redux library\n", pActive->GetString("title"));
		count++;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.
	return count;

	/*
	unsigned int count = 0;
	FileFindHandle_t testFileHandle;
	const char *pFilename = g_pFullFileSystem->FindFirst(VarArgs("%slibrary\\types\\*.key", filePath.c_str()), &testFileHandle);

	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(testFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(testFileHandle);
			continue;
		}

		std::string foundName = pFilename;
		foundName = VarArgs("library\\types\\%s", pFilename);
		pFilename = g_pFullFileSystem->FindNext(testFileHandle);

		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);

		for (int i = 0; path_buffer[i] != '\0'; i++)
			path_buffer[i] = tolower(path_buffer[i]);
		// FINISHED MAKING THE FILE PATH NICE

		foundName = path_buffer;
		if (this->LoadLocalType(foundName, filePath))
			count++;
	}

	g_pFullFileSystem->FindClose(testFileHandle);
	return count;
	*/
}

std::string C_MetaverseManager::ResolveLegacyType(std::string legacyType)
{
	if (legacyType == "")
		return AA_DEFAULT_TYPEID;

	// iterate through the types
	KeyValues* active;
	for (std::map<std::string, KeyValues*>::iterator it = m_types.begin(); it != m_types.end(); ++it)
	{
		active = this->GetActiveKeyValues(it->second);

		if (!Q_stricmp(active->GetString("title"), legacyType.c_str()))
			return it->first;
	}

	if (legacyType == "other" )
		return AA_DEFAULT_TYPEID;

	///*
	// If no type is found for this legacy type, create one!
	DevMsg("Creating a type w/ title %s because it didn't exist yet.\n", legacyType.c_str());
	KeyValues* pTypeKV = new KeyValues("type");

	// update us to 3rd generation
	pTypeKV->SetInt("generation", 3);

	// add standard info (except for id)
	pTypeKV->SetString("local/info/created", VarArgs("%llu", g_pAnarchyManager->GetTimeNumber()));
	pTypeKV->SetString("local/info/owner", "local");
	//pCurrent->SetInt("local/info/removed", 0);
	//pCurrent->SetString("local/info/remover", "");
	//pCurrent->SetString("local/info/alias", "");

	std::string typeId = g_pAnarchyManager->GenerateUniqueId();
	pTypeKV->SetString("local/info/id", typeId.c_str());

	//pCurrent->SetString("fileformat", "/.+$/i");
	//pCurrent->SetString("fileformat", "<AUTO>");	// will be processed after the apps are loaded so that file extensions can be added.
	//pCurrent->FindKey("fileformat", true);
	pTypeKV->SetString("local/titleformat", "/.+$/i");
	pTypeKV->SetString("local/title", legacyType.c_str());
	pTypeKV->SetInt("local/priority", 1);
	this->AddType(pTypeKV);
	this->SaveType(pTypeKV);	// Save to the user database, so AArcade doesnt' get confused over types when the user unsubscribes from this addon. (if they created other items using the type from this addon, for example.)
	
	return typeId;
	//*/
}

KeyValues* C_MetaverseManager::LoadLocalApp(std::string file, std::string filePath, std::string searchPath)
{
	KeyValues* pApp = new KeyValues("app");
	bool bLoaded;

//	if (filePath != "")
	//{
		std::string fullFile = filePath + file;
		bLoaded = pApp->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), searchPath.c_str());
	//}
	//else
		//bLoaded = pApp->LoadFromFile(g_pFullFileSystem, file.c_str(), "MOD");

	if (!bLoaded)
	{
		pApp->deleteThis();
		pApp = null;
	}
	else
	{
		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pApp);

		std::string id = pActive->GetString("info/id");
		m_apps[id] = pApp;
	}

	return pApp;
}

unsigned int C_MetaverseManager::LoadAllLocalInstances(sqlite3* pDb, std::map<std::string, KeyValues*>* pResponseMap)
{
	if (!pDb)
		pDb = m_db;
	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from instances", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));
	DevMsg("C_MetaverseManager::LoadAllLocalInstances\n");
	int length;
	std::string rowId;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		rowId = std::string((const char*)sqlite3_column_text(stmt, 0));
		length = sqlite3_column_bytes(stmt, 1);

		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
			continue;
		}

		KeyValues* pInstance = new KeyValues("instance");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
		pInstance->ReadAsBinary(buf);
		buf.Purge();

		//for (KeyValues *sub = pInstance->GetFirstSubKey(); sub; sub = sub->GetNextKey())
		//	DevMsg("%s is %s\n", sub->GetName(), sub->GetString());

		std::string instanceId = pInstance->GetString("info/local/id");
		//DevMsg("Instance id is: %s vs %s\n", instanceId.c_str(), rowId.c_str());
		if (instanceId == "")
		{
			DevMsg("Skipping id-less instance...\n");
			continue;
		}

		if (pResponseMap)
			(*pResponseMap)[instanceId] = pInstance;	// FIXME: Make sure these get cleaned up for ALL LoadAllLocalInstances calls.
		else
		{
			int generation = pInstance->GetInt("generation", 3);
			int iLegacy = pInstance->GetInt("legacy", 0);
			//DevMsg("But the other is: %s\n", pInstance->GetString("info/id"));
			KeyValues* pInstanceInfoKV = pInstance->FindKey("info/local", true);
			//std::string instanceId = pInstanceInfoKV->GetString("id");
			std::string mapId = pInstanceInfoKV->GetString("map");
			std::string title = pInstanceInfoKV->GetString("title");
			if (title == "")
				title = "Unnamed (" + instanceId + ")";
			std::string file = "";
			std::string workshopIds = pInstanceInfoKV->GetString(VarArgs("platforms/%s/workshopId", AA_PLATFORM_ID));
			std::string mountIds = pInstanceInfoKV->GetString(VarArgs("platforms/%s/mountIds", AA_PLATFORM_ID));
			//std::string backpackId = pInstanceInfoKV->GetString(VarArgs("platforms/%s/backpackId", AA_PLATFORM_ID));
			std::string style = pInstanceInfoKV->GetString("style");
			pInstance->deleteThis();
			pInstance = null;

			g_pAnarchyManager->GetInstanceManager()->AddInstance(iLegacy, instanceId, mapId, title, file, workshopIds, mountIds, style);

			//m_instances[instanceId] = pInstance; //DevMsg("WARNING: No response map was given to LocalAllLocalInstances!\n");
		}
		count++;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.
	return count;
}

unsigned int C_MetaverseManager::LoadAllLocalApps(sqlite3* pDb, std::map<std::string, KeyValues*>* pResponseMap)
{
	if (!pDb)
		pDb = m_db;
	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from apps", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	int length;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
			continue;
		}

		KeyValues* pApp = new KeyValues("app");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
		pApp->ReadAsBinary(buf);
		buf.Purge();

		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pApp);
		std::string id = pActive->GetString("info/id");

		if (pResponseMap)
			(*pResponseMap)[id] = pApp;
		else
			m_apps[id] = pApp;

		count++;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.
	return count;
}

std::string C_MetaverseManager::ResolveLegacyApp(std::string legacyApp)
{
	if (legacyApp == "")
		return "";

	// iterate through the types
	KeyValues* active;
	for (std::map<std::string, KeyValues*>::iterator it = m_apps.begin(); it != m_apps.end(); ++it)
	{
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(it->second);
		if (!Q_stricmp(active->GetString("title"), legacyApp.c_str()))
			return it->first;
	}

	return "";
}

KeyValues* C_MetaverseManager::LoadFirstLocalApp(std::string filePath)
{
	if (m_previousLoadLocalAppFilePath != "")
		this->LoadLocalAppClose();

	m_previousLoadLocalAppFilePath = filePath;
	const char *pFilename = g_pFullFileSystem->FindFirstEx("library\\apps\\*.key", filePath.c_str(), &m_previousLoadLocalAppFileHandle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalAppFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalAppFileHandle);
			continue;
		}

		std::string foundName = pFilename;
		foundName = VarArgs("library\\apps\\%s", pFilename);
		
		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);

		for (int i = 0; path_buffer[i] != '\0'; i++)
			path_buffer[i] = tolower(path_buffer[i]);
		// FINISHED MAKING THE FILE PATH NICE

		foundName = path_buffer;
		return this->LoadLocalApp(foundName, "", m_previousLoadLocalAppFilePath);
	}

	return null;
}

KeyValues* C_MetaverseManager::LoadNextLocalApp()
{
//	if ( != "")
//		g_pFullFileSystem->FindClose(m_previousLoadLocalAppFileHandle);

	const char *pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalAppFileHandle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(m_previousLoadLocalAppFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(m_previousLoadLocalAppFileHandle);
			continue;
		}

		std::string foundName = pFilename;
		foundName = VarArgs("library\\apps\\%s", pFilename);

		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);

		for (int i = 0; path_buffer[i] != '\0'; i++)
			path_buffer[i] = tolower(path_buffer[i]);
		// FINISHED MAKING THE FILE PATH NICE

		foundName = path_buffer;
		return this->LoadLocalApp(foundName, "", m_previousLoadLocalAppFilePath);
	}

	//g_pFullFileSystem->FindClose(m_previousLoadLocalAppFileHandle);
	//m_previousLoadLocalAppFilePath = "";
	return null;
}

void C_MetaverseManager::LoadLocalAppClose()
{
	if (m_previousLoadLocalAppFilePath != "")
	{
		g_pFullFileSystem->FindClose(m_previousLoadLocalAppFileHandle);
		m_previousLoadLocalAppFilePath = "";
	}
}

KeyValues* C_MetaverseManager::GetFirstLibraryType()
{
	m_previousGetTypeIterator = m_types.begin();
	if (m_previousGetTypeIterator != m_types.end())
		return m_previousGetTypeIterator->second;

	return null;
}

KeyValues* C_MetaverseManager::GetNextLibraryType()
{
	if (m_previousGetTypeIterator != m_types.end())
	{
		m_previousGetTypeIterator++;
		
		if (m_previousGetTypeIterator != m_types.end())
			return m_previousGetTypeIterator->second;
	}

	return null;
}

KeyValues* C_MetaverseManager::GetFirstLibraryApp()
{
	m_previousGetAppIterator = m_apps.begin();
	if (m_previousGetAppIterator != m_apps.end())
		return m_previousGetAppIterator->second;

	return null;
}

KeyValues* C_MetaverseManager::GetNextLibraryApp()
{
	if (m_previousGetAppIterator != m_apps.end())
	{
		m_previousGetAppIterator++;

		if (m_previousGetAppIterator != m_apps.end())
			return m_previousGetAppIterator->second;
	}

	return null;
}

/*
KeyValues* C_MetaverseManager::FindLibraryType(std::string term)
{
	// note: only finds exact matches.  IS case sensi
	auto it = m_types.begin();
	while (it != m_types.end())
	{
		if (it->first == term)
			return it->second;

		it++;
	}

	return null;
}
*/

KeyValues* C_MetaverseManager::GetLibraryType(std::string id)
{
	std::map<std::string, KeyValues*>::iterator it = m_types.find(id);
	if (it != m_types.end())
		return it->second;
	else
		return null;
}

const char* C_MetaverseManager::GetFirstLibraryEntry(KeyValues*& response, const char* category)
{
	//char queryId[20];
	//const char* queryId = g_pAnarchyManager->GenerateUniqueId();
	//g_pAnarchyManager->GenerateUniqueId(queryId);

	//const char*
	std::string queryId = g_pAnarchyManager->GenerateUniqueId();
	DevMsg("Here id is: %s\n", queryId.c_str());

	//std::string idBuf = std::string(queryId);

	// TODO: use queryId to organize N search queries & store the category info for each.
	//*
	if (!Q_strcmp(category, "items"))
	{
		m_previousGetItemIterator = m_items.begin();
		response = (m_previousGetItemIterator != m_items.end()) ? m_previousGetItemIterator->second : null;
	}
	else // models
	{
		m_previousGetModelIterator = m_models.begin();
		response = (m_previousGetModelIterator != m_models.end()) ? m_previousGetModelIterator->second : null;
	}
	//*/

	/*
	auto categoryEntries = (!Q_strcmp(category, "items")) ? m_items : m_models;
	auto it = (!Q_strcmp(category, "items")) ? m_previousGetItemIterator : m_previousGetModelIterator;

	it = categoryEntries.begin();

	if (it != categoryEntries.end())
		response = it->second;
	else
		response = null;
	*/

	return queryId.c_str();
}
KeyValues* C_MetaverseManager::GetNextLibraryEntry(const char* queryId, const char* category)
{
	///*
	//DevMsg("AAaaannnd here it is: %s and %s\n", queryId, category);
	if (!Q_strcmp(category, "items"))
	{
		//DevMsg("Has it: %i\n", m_previousGetItemIterator);
		if (m_previousGetItemIterator != m_items.end())
			m_previousGetItemIterator++;

		return (m_previousGetItemIterator != m_items.end()) ? m_previousGetItemIterator->second : null;
	}
	else	// models
	{
		if (m_previousGetModelIterator != m_models.end())
			m_previousGetModelIterator++;

		return (m_previousGetModelIterator != m_models.end()) ? m_previousGetModelIterator->second : null;
	}
	//*/

	/*
	// TODO: use queryId to organize N search queries & store the category info for each.
	auto categoryEntries = (!Q_strcmp(category, "items")) ? m_items : m_models;
	auto it = (!Q_strcmp(category, "items")) ? m_previousGetItemIterator : m_previousGetModelIterator;

	if (it != categoryEntries.end())
	{
		it++;

		if (it != categoryEntries.end())
			return it->second;
	}
	*/

	return null;
}

// LEGACY! OBSOLETE!!
KeyValues* C_MetaverseManager::GetFirstLibraryItem()
{
	m_previousGetItemIterator = m_items.begin();
	if (m_previousGetItemIterator != m_items.end())
		return m_previousGetItemIterator->second;

	return null;
}

KeyValues* C_MetaverseManager::GetNextLibraryItem()
{
	if (m_previousGetItemIterator != m_items.end())
	{
		m_previousGetItemIterator++;

		if (m_previousGetItemIterator != m_items.end())
			return m_previousGetItemIterator->second;
	}

	return null;
}

KeyValues* C_MetaverseManager::GetFirstLibraryModel()
{
	m_previousGetModelIterator = m_models.begin();
	if (m_previousGetModelIterator != m_models.end())
		return m_previousGetModelIterator->second;

	return null;
}

KeyValues* C_MetaverseManager::GetNextLibraryModel()
{
	if (m_previousGetModelIterator != m_models.end())
	{
		m_previousGetModelIterator++;

		if (m_previousGetModelIterator != m_models.end())
			return m_previousGetModelIterator->second;
	}

	return null;
}

KeyValues* C_MetaverseManager::GetLibraryItem(std::string id)
{
	std::map<std::string, KeyValues*>::iterator it = m_items.find(id);
	if (it != m_items.end())
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::GetLibraryModel(std::string id)
{
	std::map<std::string, KeyValues*>::iterator it = m_models.find(id);
	if (it != m_models.end())
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::FindLibraryModel(KeyValues* pSearchInfo, std::map<std::string, KeyValues*>::iterator& it)
{
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	//unsigned int i, numTokens;
	bool bFoundMatch;
	while (it != m_models.end())
	{
		bFoundMatch = false;
		potential = it->second;
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(potential);
		// active has the potential model data
		// pSearchInfo has the search criteria
		bool bGood = false;
		for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
		{
			fieldName = searchField->GetName();
			if (fieldName == "title")
			{
				if (!Q_strcmp(searchField->GetString(), ""))
					bGood = true;
				else
				{
					potentialBuf = active->GetString("title");
					std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

					searchBuf = searchField->GetString();
					std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

					/*
					searchTokens.clear();
					g_pAnarchyManager->Tokenize(searchBuf, searchTokens, " ");
					numTokens = searchTokens.size();

					for (i = 0; i < numTokens; i++)
					{
					if (searchTokens[i].length() < 2)
					continue;

					if (potentialBuf.find(searchTokens[i]) != std::string::npos)
					{
					bFoundMatch = true;
					break;
					}
					}
					*/

					if (potentialBuf.find(searchBuf) != std::string::npos)
						bGood = true;
					else
						bGood = false;
				}
			}
			else if (fieldName == "dynamic")
			{
				if (!Q_strcmp(searchField->GetString(), "") || !Q_strcmp(active->GetString("dynamic"), searchField->GetString()))
					bGood = true;
				else
					bGood = false;
			}

			if (!bGood)
				break;
		}

		if (bGood)
		{
			bFoundMatch = true;
			break;
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	if (bFoundMatch)
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::FindLibraryModel(KeyValues* pSearchInfo)
{
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	unsigned int i, numTokens;
	bool bFoundMatch;
	std::map<std::string, KeyValues*>::iterator it = m_models.begin();
	while (it != m_models.end())
	{
		bFoundMatch = false;
		potential = it->second;
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(potential);
		// active has the potential model data
		// pSearchInfo has the search criteria
		for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
		{
			fieldName = searchField->GetName();
			if (fieldName == "title")
			{
				potentialBuf = searchField->GetString();	// FIXME: Does this work? seems to work... but how?
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

				searchBuf = pSearchInfo->GetString(fieldName.c_str());
				searchTokens.clear();
				g_pAnarchyManager->Tokenize(searchBuf, searchTokens, ", ");
				numTokens = searchTokens.size();

				for (i = 0; i < numTokens; i++)
				{
					if (searchTokens[i].length() < 2)
						continue;

					if (potentialBuf.find(searchTokens[i]) != std::string::npos)
					{
						bFoundMatch = true;
						break;
					}
				}

				if (bFoundMatch)
					break;
			}
			else if (fieldName == "file")
			{
				potentialBuf = active->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID));
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);
				std::replace(potentialBuf.begin(), potentialBuf.end(), '/', '\\'); // replace all '/' to '\'

				searchBuf = searchField->GetString();
				std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);
				std::replace(searchBuf.begin(), searchBuf.end(), '/', '\\'); // replace all '/' to '\'

				if (potentialBuf == searchBuf)
				{
					//	DevMsg("Found match with %s = %s\n", potentialBuf.c_str(), searchBuf.c_str());
					bFoundMatch = true;
					break;
				}
			}
			else
			{
				potentialBuf = active->GetString(fieldName.c_str());
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

				searchBuf = searchField->GetString();
				std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

				if (potentialBuf == searchBuf)
				{
					//	DevMsg("Found match with %s = %s\n", potentialBuf.c_str(), searchBuf.c_str());
					bFoundMatch = true;
					break;
				}
			}
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	// Do this here because we MUST handle the deletion for findNext and findFirst, so this usage should match!!
	pSearchInfo->deleteThis();

	if (bFoundMatch)
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::FindFirstLibraryModel(KeyValues* pSearchInfo)
{
	// remember this search query
	if (!m_pPreviousModelSearchInfo)
		m_pPreviousModelSearchInfo = pSearchInfo;// new KeyValues("search");
	else if (m_pPreviousModelSearchInfo != pSearchInfo)	// this should never be called!!!
	{
		m_pPreviousModelSearchInfo->deleteThis();
		m_pPreviousModelSearchInfo = pSearchInfo;
	}

	m_previousFindModelIterator = m_models.begin();

	// start the search
	KeyValues* response = this->FindLibraryModel(m_pPreviousModelSearchInfo, m_previousFindModelIterator);
	return response;
}

KeyValues* C_MetaverseManager::FindNextLibraryModel()
{
	// continue the search
	KeyValues* response = null;
	m_previousFindModelIterator++;
	if (m_previousFindModelIterator != m_models.end())
		response = this->FindLibraryModel(m_pPreviousModelSearchInfo, m_previousFindModelIterator);
	return response;
}

/*
KeyValues* C_MetaverseManager::FindLibraryModel(KeyValues* pSearchInfo, bool bExactOnly)
{
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	unsigned int i, numTokens;
	bool bFoundMatch;
	std::map<std::string, KeyValues*>::iterator it = m_models.begin();
	char slashBuffer[AA_MAX_STRING];
	while (it != m_models.end())
	{
		bFoundMatch = false;
		potential = it->second;
		active = potential->FindKey("current");
		if (!active)
			active = potential->FindKey("local");
		if (active)
		{
			// active has the potential model data
			// pSearchInfo has the search criteria
			for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
			{
				fieldName = searchField->GetName();
				if (fieldName == "lastmodel")
				{
					if (!bExactOnly)
					{
						potentialBuf = searchField->GetString();
						std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

						searchBuf = pSearchInfo->GetString(fieldName.c_str());
						searchTokens.clear();
						g_pAnarchyManager->Tokenize(searchBuf, searchTokens, ", ");
						numTokens = searchTokens.size();

						for (i = 0; i < numTokens; i++)
						{
							if (searchTokens[i].length() < 2)
								continue;

							if (potentialBuf.find(searchTokens[i]) != std::string::npos)
							{
								bFoundMatch = true;
								break;
							}
						}
					}
					else
					{
						// fix slashes to reduce duplicates
						Q_strcpy(slashBuffer, searchField->GetString());
						V_FixSlashes(slashBuffer);
						potentialBuf = slashBuffer;

						Q_strcpy(slashBuffer, pSearchInfo->GetString(fieldName.c_str()));
						V_FixSlashes(slashBuffer);
						searchBuf = slashBuffer;

						if (potentialBuf == searchBuf)
							bFoundMatch = true;
					}

					if (bFoundMatch)
						break;

				}
			}
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	// Do this here because we MUST handle the deletion for findNext and findFirst, so this usage should match!!
	pSearchInfo->deleteThis();

	if (bFoundMatch)
		return it->second;
	else
		return null;
}
*/

KeyValues* C_MetaverseManager::FindLibraryEntry(const char* category, KeyValues* pSearchInfo, std::map<std::string, KeyValues*>::iterator& it)
{
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	//unsigned int i, numTokens;
	bool bFoundMatch;

	std::map<std::string, KeyValues*>* pCategoryEntries = (!Q_strcmp(category, "items")) ? &m_items : &m_models;

	while (it != pCategoryEntries->end())
	{
		bFoundMatch = false;
		active = this->GetActiveKeyValues(it->second);
		if (active)
		{
			// active has the potential item data
			// pSearchInfo has the search criteria
			bool bGood = false;
			for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
			{
				fieldName = searchField->GetName();
				if (fieldName == "title")
				{
					if (!Q_strcmp(searchField->GetString(), ""))
						bGood = true;
					else
					{
						potentialBuf = active->GetString("title");
						std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

						searchBuf = searchField->GetString();
						std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

						/*
						searchTokens.clear();
						g_pAnarchyManager->Tokenize(searchBuf, searchTokens, " ");
						numTokens = searchTokens.size();

						for (i = 0; i < numTokens; i++)
						{
						if (searchTokens[i].length() < 2)
						continue;

						if (potentialBuf.find(searchTokens[i]) != std::string::npos)
						{
						bFoundMatch = true;
						break;
						}
						}
						*/

						if (potentialBuf.find(searchBuf) != std::string::npos)
							bGood = true;
						else
							bGood = false;
					}
				}
				/*
				else if (categoryEntries == m_models && fieldName == "dynamic")
				{
					if (!Q_strcmp(searchField->GetString(), "") || !Q_strcmp(active->GetString("dynamic"), searchField->GetString()))
						bGood = true;
					else
						bGood = false;
				}*/
				else
				{
					potentialBuf = active->GetString(fieldName.c_str());
					std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

					searchBuf = searchField->GetString();
					std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

					if (potentialBuf == searchBuf)
						bGood = true;
					else
						bGood = false;
				}
				/*
				else if (fieldName == "type")
				{
				if (!Q_strcmp(searchField->GetString(), "") || !Q_strcmp(active->GetString("type"), searchField->GetString()))
				bGood = true;
				else
				bGood = false;
				}
				*/

				if (!bGood)
					break;
			}

			if (bGood)
			{
				bFoundMatch = true;
				break;
			}
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	if (bFoundMatch)
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::FindLibraryItem(KeyValues* pSearchInfo, std::map<std::string, KeyValues*>::iterator& it)
{
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	//unsigned int i, numTokens;
	bool bFoundMatch;
	while (it != m_items.end())
	{
		bFoundMatch = false;
		potential = it->second;
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(potential);
		// active has the potential item data
		// pSearchInfo has the search criteria
		bool bGood = false;
		for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
		{
			fieldName = searchField->GetName();
			if (fieldName == "title")
			{
				if (!Q_strcmp(searchField->GetString(), ""))
					bGood = true;
				else
				{
					potentialBuf = active->GetString("title");
					std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

					searchBuf = searchField->GetString();
					std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

					/*
					searchTokens.clear();
					g_pAnarchyManager->Tokenize(searchBuf, searchTokens, " ");
					numTokens = searchTokens.size();

					for (i = 0; i < numTokens; i++)
					{
					if (searchTokens[i].length() < 2)
					continue;

					if (potentialBuf.find(searchTokens[i]) != std::string::npos)
					{
					bFoundMatch = true;
					break;
					}
					}
					*/

					if (potentialBuf.find(searchBuf) != std::string::npos)
						bGood = true;
					else
						bGood = false;
				}
			}
			else
			{
				potentialBuf = active->GetString(fieldName.c_str());
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

				searchBuf = searchField->GetString();
				std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

				if (potentialBuf == searchBuf)
					bGood = true;
				else
					bGood = false;
			}
			/*
			else if (fieldName == "type")
			{
				if (!Q_strcmp(searchField->GetString(), "") || !Q_strcmp(active->GetString("type"), searchField->GetString()))
					bGood = true;
				else
					bGood = false;
			}
			*/

			if (!bGood)
				break;
		}

		if (bGood)
		{
			bFoundMatch = true;
			break;
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	if (bFoundMatch)
		return it->second;
	else
		return null;
}

KeyValues* C_MetaverseManager::FindLibraryItem(KeyValues* pSearchInfo)
{
	//DevMsg("C_MetaverseManager: FindLibraryItem with ONLY pSearchinfo!!\n");
	KeyValues* potential;
	KeyValues* active;
	KeyValues* searchField;
	std::string fieldName, potentialBuf, searchBuf;
	char charBuf[AA_MAX_STRING];
	std::vector<std::string> searchTokens;
	unsigned int i, numTokens;
	bool bFoundMatch = false;
	std::map<std::string, KeyValues*>::iterator it = m_items.begin();
	while (it != m_items.end())
	{
		bFoundMatch = false;
		potential = it->second;
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(potential);
		// active has the potential item data
		// pSearchInfo has the search criteria
		for (searchField = pSearchInfo->GetFirstSubKey(); searchField; searchField = searchField->GetNextKey())
		{
			fieldName = searchField->GetName();
			if (fieldName == "title")
			{
				potentialBuf = searchField->GetString();	// FIXME: Does this work? seems to work... but how?
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

				searchBuf = pSearchInfo->GetString(fieldName.c_str());
				searchTokens.clear();
				g_pAnarchyManager->Tokenize(searchBuf, searchTokens, ", ");
				numTokens = searchTokens.size();

				for (i = 0; i < numTokens; i++)
				{
					if (searchTokens[i].length() < 2)
						continue;

					if (potentialBuf.find(searchTokens[i]) != std::string::npos)
					{
						bFoundMatch = true;
						break;
					}
				}

				if (bFoundMatch)
					break;
			}
			else
			{
				potentialBuf = active->GetString(fieldName.c_str());
				std::transform(potentialBuf.begin(), potentialBuf.end(), potentialBuf.begin(), ::tolower);

				searchBuf = searchField->GetString();
				std::transform(searchBuf.begin(), searchBuf.end(), searchBuf.begin(), ::tolower);

				if (potentialBuf == searchBuf)
				{
				//	DevMsg("Found match with %s = %s\n", potentialBuf.c_str(), searchBuf.c_str());
					bFoundMatch = true;
					break;
				}
			}
		}

		if (bFoundMatch)
			break;
		else
			it++;
	}

	// Do this here because we MUST handle the deletion for findNext and findFirst, so this usage should match!!
	pSearchInfo->deleteThis();

	if (bFoundMatch)
		return it->second;
	else
		return null;
}

std::string C_MetaverseManager::FindFirstLibraryEntry(KeyValues*& response, const char* category, KeyValues* pSearchInfo)
{
	//std::string categoryBuf = std::string(category);

	//if (categoryBuf == "")
	//	categoryBuf = "items";

	std::string queryId = g_pAnarchyManager->GenerateUniqueId();
	//DevMsg("Query ID point A: %s\n", queryId.c_str());
	//std::string idBuf = std::string(queryId);

	// TODO: use queryId to organize N search queries & store the category info for each.
	KeyValues* pEntry;
	KeyValues* pPreviousSearchInfo = null;
	std::map<std::string, KeyValues*>::iterator* it;
	std::map<std::string, KeyValues*>* categoryEntries;
	if (!Q_strcmp(category, "items"))
	{
		categoryEntries = &m_items;

		if (m_pPreviousSearchInfo)
		{
			DevMsg("Cleaning up a library query context that was left open...\n");	// FIXME: This entire block should be handled differently as soon as concurrent library queries are supported, but right now we're actually limited to 1 query per category.
			pPreviousSearchInfo->deleteThis();
		}

		m_pPreviousSearchInfo = pSearchInfo;
		it = &m_previousFindItemIterator;
	}
	else// if (categoryBuf == "models")
	{
		categoryEntries = &m_models;

		if (m_pPreviousModelSearchInfo)
		{
			DevMsg("Cleaning up a library query context that was left open...\n");	// FIXME: This entire block should be handled differently as soon as concurrent library queries are supported, but right now we're actually limited to 1 query per category.
			m_pPreviousModelSearchInfo->deleteThis();
		}

		m_pPreviousModelSearchInfo = pSearchInfo;
		it = &m_previousFindModelIterator;
	}
	
	// start the search
	//m_previousFindItemIterator = categoryEntries->begin();
	*it = categoryEntries->begin();
	pEntry = this->FindLibraryEntry(category, pSearchInfo, *it);

	response = pEntry;
	//std::string idBuf = std::string(queryId);
	return queryId.c_str();
}

KeyValues* C_MetaverseManager::FindNextLibraryEntry(const char* queryId, const char* category)
{
	KeyValues* response = null;
	if (!Q_strcmp(category, "items"))
	{
		// continue the search
		m_previousFindItemIterator++;
		if (m_previousFindItemIterator != m_items.end())
			response = this->FindLibraryEntry(category, m_pPreviousSearchInfo, m_previousFindItemIterator);
	}
	else
	{
		// continue the search
		m_previousFindModelIterator++;
		if (m_previousFindModelIterator != m_models.end())
			response = this->FindLibraryEntry(category, m_pPreviousModelSearchInfo, m_previousFindModelIterator);
	}

	return response;
}

KeyValues* C_MetaverseManager::FindFirstLibraryItem(KeyValues* pSearchInfo)
{
	// remember this search query
	if (!m_pPreviousSearchInfo)
		m_pPreviousSearchInfo = pSearchInfo;// new KeyValues("search");
	else if (m_pPreviousSearchInfo != pSearchInfo)	// this should never be called!!!
	{
		m_pPreviousSearchInfo->deleteThis();
		m_pPreviousSearchInfo = pSearchInfo;
	}

	m_previousFindItemIterator = m_items.begin();

	// start the search
	KeyValues* response = this->FindLibraryItem(m_pPreviousSearchInfo, m_previousFindItemIterator);
	return response;
}

KeyValues* C_MetaverseManager::FindNextLibraryItem()
{
	// continue the search
	KeyValues* response = null;
	m_previousFindItemIterator++;
	if (m_previousFindItemIterator != m_items.end())
		response = this->FindLibraryItem(m_pPreviousSearchInfo, m_previousFindItemIterator);
	return response;
}

KeyValues* C_MetaverseManager::GetScreenshot(std::string id)
{
	std::map<std::string, KeyValues*>::iterator it = m_mapScreenshots.find(id);
	if (it != m_mapScreenshots.end())
		return it->second;
	else
		return null;
}

void C_MetaverseManager::AddScreenshot(KeyValues* pScreenshotKV)
{
	std::string id = pScreenshotKV->GetString("id");
	m_mapScreenshots[id] = pScreenshotKV;
}

void C_MetaverseManager::DeleteScreenshot(std::string id)
{
	std::string filename;
	KeyValues* pScreenshotKV = this->GetScreenshot(id);
	if (pScreenshotKV)
	{
		// 1. Delete [ID].tga and [ID].txt from the shots folder.
		filename = "shots\\" + id + ".tga";
		if (g_pFullFileSystem->FileExists(filename.c_str(), "DEFAULT_WRITE_PATH"))
			g_pFullFileSystem->RemoveFile(filename.c_str(), "DEFAULT_WRITE_PATH");

		filename = "shots\\" + id + ".txt";
		if (g_pFullFileSystem->FileExists(filename.c_str(), "DEFAULT_WRITE_PATH"))
			g_pFullFileSystem->RemoveFile(filename.c_str(), "DEFAULT_WRITE_PATH");

		// 2. Remove the entry from m_mapScreenshots & delete the KeyValues.
		pScreenshotKV->deleteThis();
		m_mapScreenshots.erase(id);
	}
}

KeyValues* C_MetaverseManager::FindMostRecentScreenshot(std::string mapId, instance_t* pInstance)
{
	std::string instanceId = (pInstance) ? pInstance->id : "";

	KeyValues* pMapKV = null;
	std::string mapFileName = "";
	std::string goodMapId = "";
	if (mapId != "")
	{
		goodMapId = mapId;
		pMapKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(this->GetMap(goodMapId));
		if (pMapKV)
		{
			goodMapId = pMapKV->GetString("info/id");
			mapFileName = pMapKV->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID));
		}
	}
	else
	{
		mapFileName = std::string(g_pAnarchyManager->MapName()) + std::string(".bsp");
		pMapKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(this->FindMap(mapFileName.c_str()));
		if (pMapKV)
			goodMapId = pMapKV->GetString("info/id");
	}

	if (goodMapId == "" || mapFileName == "" )
		return null;

	//std::string testId;
	std::vector<KeyValues*> screenshots;
	std::map<std::string, KeyValues*>::iterator it = m_mapScreenshots.begin();
	while (it != m_mapScreenshots.end())
	{
		//testId = it->second->GetString("map/id");	// SOMEHOW COULD BE 1.#INF00 when printed as a string, even though it was fine in the KV text. Like for ID 545e1841
		//testId = (instanceId == "") ? it->second->GetString("instance/mapId") : it->second->GetString("instance/id");
		//DevMsg("IDs: %s vs %s\n", testId.c_str(), goodMapId.c_str());
		//if ((instanceId == "" && testId == goodMapId) || (instanceId != "" && testId == instanceId))
		if (!Q_strcmp(it->second->GetString("map/file"), mapFileName.c_str()))
			screenshots.push_back(it->second);

		it++;
	}

	// now screenshots holds all applicable screenshots.  find the most recent one.
	KeyValues* pBestScreenshot = null;
	uint64 bestValue = 0;
	uint64 testValue;
	unsigned int max = screenshots.size();
	for (unsigned int i = 0; i < max; i++)
	{
		testValue = Q_atoui64(screenshots[i]->GetString("created"));
		if (testValue > bestValue)
		{
			bestValue = testValue;
			pBestScreenshot = screenshots[i];
		}
	}

	return pBestScreenshot;
}

KeyValues* C_MetaverseManager::GetLibraryApp(std::string id)
{
	std::map<std::string, KeyValues*>::iterator it = m_apps.find(id);
	if (it != m_apps.end())
		return it->second;
	else
		return null;
}

std::map<std::string, KeyValues*>& C_MetaverseManager::DetectAllMapScreenshots()
{
	DevMsg("Detecting all map screenshots...\n");
	size_t found;
	std::string id;
	//std::map<std::string, std::string>::iterator it;

	// get every shot that exists
	KeyValues* pShotKV;
	FileFindHandle_t pFileFindHandle;
	const char *pScreenshotInfoFile = g_pFullFileSystem->FindFirstEx("shots\\*.txt", "MOD", &pFileFindHandle);
	while (pScreenshotInfoFile != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(pFileFindHandle))
		{
			pScreenshotInfoFile = g_pFullFileSystem->FindNext(pFileFindHandle);
			continue;
		}

		id = pScreenshotInfoFile;
		id = id.substr(0, id.find(".txt"));

		pShotKV = new KeyValues("screenshot");
		if (pShotKV->LoadFromFile(g_pFullFileSystem, VarArgs("shots\\%s", pScreenshotInfoFile), "MOD"))
		{
			// IMPORTANT NOTE: Key values loaded with LoadFromFile forget the data types for their fields and if a string is just numbers, it gets turned into a number instead of a string after loading.
			// This matters if the number started with a 0, because leading zeros get removed for numbers.
			// So to catch this, additional checks must be performed on ID's read from KeyValues files.
			//DevMsg("Here the map ID for the screenshot is: %s or %i or %s and type %s\n", pShotKV->GetString("map/id"), pShotKV->GetInt("map/id"), pShotKV->GetRawString("map/id"), pShotKV->GetDataType("map/id"));
			m_mapScreenshots[id] = pShotKV;
		}
		else
			pShotKV->deleteThis();
		
		pScreenshotInfoFile = g_pFullFileSystem->FindNext(pFileFindHandle);
	}
	g_pFullFileSystem->FindClose(pFileFindHandle);

	return m_mapScreenshots;
}

void C_MetaverseManager::GetAllMapScreenshots(std::vector<KeyValues*>& responseVector, std::string mapId)
{
	std::map<std::string, KeyValues*>::iterator it = m_mapScreenshots.begin();
	while (it != m_mapScreenshots.end())
	{
		//DevMsg("Screenshot check: %s vs %s vs %i\n", mapId.c_str(), it->second->GetString("map/id"), it->second->GetInt("map/id"));
		//if (mapId == "" || mapId == it->second->GetString("map/id"))//; it->first)
		if (mapId == "" || g_pAnarchyManager->CompareLoadedFromKeyValuesFileId(it->second->GetString("map/id"), mapId.c_str()) )
			responseVector.push_back(it->second);

		it++;
	}
}

KeyValues* C_MetaverseManager::FindMap(const char* mapFile)
{
	KeyValues* map;
	std::map<std::string, KeyValues*>::iterator it = m_maps.begin();
	while (it != m_maps.end())
	{
		map = this->GetActiveKeyValues(it->second);
		//DevMsg("Map name is: %s\n", map->GetString("platforms/-KJvcne3IKMZQTaG7lPo/file"));
		if (!Q_stricmp(map->GetString("platforms/-KJvcne3IKMZQTaG7lPo/file"), mapFile))
			return it->second;

		it++;
	}

	return null;
}

KeyValues* C_MetaverseManager::GetMap(std::string mapId)
{
	std::map<std::string, KeyValues*>::iterator it = m_maps.find(mapId);
	if (it != m_maps.end())
		return it->second;
	return null;
}

void C_MetaverseManager::DetectAllLegacyCabinets()
{
	std::string goodTitle;
	std::string modelId;
	KeyValues* pModel;
	KeyValues* cat = new KeyValues("cat");
	FileFindHandle_t handle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx("resource\\models\\*.cat", "GAME", &handle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(handle))
		{
			pFilename = g_pFullFileSystem->FindNext(handle);
			continue;
		}

		// load up the cat
		cat->Clear();
		if (cat->LoadFromFile(g_pFullFileSystem, VarArgs("resource\\models\\%s", pFilename), "GAME"))
		{
			modelId = g_pAnarchyManager->GenerateLegacyHash(cat->GetString("model"));

			std::map<std::string, KeyValues*>::iterator oldIt = m_models.find(modelId);
			if (oldIt == m_models.end())
			{
				pModel = new KeyValues("model");

				// update us to 3rd generation
				pModel->SetInt("generation", 3);

				// add standard info (except for id)
				pModel->SetInt("local/info/created", 0);
				pModel->SetString("local/info/owner", "local");
				pModel->SetInt("local/info/removed", 0);
				//pModel->SetString("local/info/remover", "");
				//pModel->SetString("local/info/alias", "");


				pModel->SetString("local/info/id", modelId.c_str());

				pModel->SetString("local/title", cat->GetString("name"));
				pModel->SetString("local/keywords", cat->GetString("category"));
				pModel->SetInt("local/dynamic", 1);
				pModel->SetString(VarArgs("local/platforms/%s/id", AA_PLATFORM_ID), AA_PLATFORM_ID);
				pModel->SetString(VarArgs("local/platforms/%s/file", AA_PLATFORM_ID), cat->GetString("model"));
				//pModel->SetString(VarArgs("local/platforms/%s/download", AA_PLATFORM_ID), "");

				//pModel->SetString(VarArgs("local/platforms/%s/workshopId", AA_PLATFORM_ID), "");
				//pModel->SetString(VarArgs("local/platforms/%s/mountId", AA_PLATFORM_ID), "");

				// models can be loaded right away because they don't depend on anything else, like items do. (items depend on models)
				DevMsg("Loading cabinet model with ID %s and model %s\n", modelId.c_str(), cat->GetString("model"));
				m_models[modelId] = pModel;
			}
		}

		pFilename = g_pFullFileSystem->FindNext(handle);
	}

	g_pFullFileSystem->FindClose(handle);
	cat->deleteThis();
}

void C_MetaverseManager::DetectAllMaps()
{
	DevMsg("C_MetaverseManager::DetectAllMaps\n");	// fix me down there at the todo!!

	// Load all the .key files from the library/instances folder.
	// then add their instances:

	/* done elsewhere now
	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(m_db, "SELECT * from instances", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(m_db));

	// TODO: Fix these things to have real values!!  This is very much like the logic done ELSEWHERE!! FIND IT!!

	int length;
	int iGeneration;
	int iLegacy;
	std::string instanceId;
	std::string mapId;
	std::string title;
	std::string file;
	std::string workshopIds;
	std::string mountIds;
	//std::string backpackId;
	std::string style;
	KeyValues* pInstanceKV;
	KeyValues* pInstanceInfoKV;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		// FIXME: TODO: Detect if the map is from a workshop or mount id too!
		pInstanceKV = new KeyValues("instance");
		if (this->LoadSQLKevValues("instances", (const char*)sqlite3_column_text(stmt, 0), pInstanceKV))
		{
			iGeneration = pInstanceKV->GetInt("generation", 3);
			iLegacy = pInstanceKV->GetInt("legacy");

			pInstanceInfoKV = pInstanceKV->FindKey("info/local", true);
			instanceId = pInstanceInfoKV->GetString("id");

			if (instanceId == "")
			{
				DevMsg("Warning: Skipping instance with no ID.\n");
				continue;
			}

			mapId = pInstanceInfoKV->GetString("map");
			title = pInstanceInfoKV->GetString("title");
			if (title == "")
				title = "Unnamed (" + instanceId + ")";
			file = "";
			workshopIds = pInstanceInfoKV->GetString(VarArgs("platforms/%s/workshopId", AA_PLATFORM_ID));
			mountIds = pInstanceInfoKV->GetString(VarArgs("platforms/%s/mountIds", AA_PLATFORM_ID));
			//backpackId = pInstanceInfoKV->GetString(VarArgs("platforms/%s/backpackId", AA_PLATFORM_ID));
			style = pInstanceInfoKV->GetString("style");

			g_pAnarchyManager->GetInstanceManager()->AddInstance(iLegacy, instanceId, mapId, title, file, workshopIds, mountIds, style);// (kv->GetInt("legacy"), instanceId, mapId, title, file, workshopIds, mountIds, style);//(instanceId, kv->GetString("info/map"), goodTitle, goodLegacyFile);
		}
		pInstanceKV->deleteThis();
		pInstanceKV = null;
	}
	sqlite3_finalize(stmt);
	*/

	C_AwesomiumBrowserInstance* pHudBrowserInstance = g_pAnarchyManager->GetAwesomiumBrowserManager()->FindAwesomiumBrowserInstance("hud");
	bool bAlreadyExists;

	DevMsg("Detect first map...\n");
	KeyValues* map = this->DetectFirstMap(bAlreadyExists);
	if (map)
	{
		if (bAlreadyExists)
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Detecting Maps", "detectmaps", "", "", "+0", "detectNextMapCallback");
		else
			pHudBrowserInstance->AddHudLoadingMessage("progress", "", "Detecting Maps", "detectmaps", "", "", "+", "detectNextMapCallback");
	}
	else
		this->OnDetectAllMapsCompleted();

	DevMsg("Initial logic for DetectAllMaps complete.\n");
}

void C_MetaverseManager::OnDetectAllMapsCompleted()
{
	DevMsg("Done detecting maps!\n");

	g_pAnarchyManager->GetMetaverseManager()->DetectAllMapScreenshots();
	g_pAnarchyManager->OnDetectAllMapsComplete();
}

KeyValues* C_MetaverseManager::GetActiveKeyValues(KeyValues* entry)
{
	// return null if given a null argument to avoid the if-then cluster fuck of error checking each step of this common task
	if (!entry)
		return null;

	KeyValues* active = entry->FindKey("current");
	if (!active)
		active = entry->FindKey("local", true);

	return active;
}

void C_MetaverseManager::SetLibraryBrowserContext(std::string category, std::string id, std::string search, std::string filter)
{
	m_libraryBrowserContextCategory = category;
	m_libraryBrowserContextId = id;
	m_libraryBrowserContextSearch = search;
	m_libraryBrowserContextFilter = filter;
}

std::string C_MetaverseManager::GetLibraryBrowserContext(std::string name)
{
	if (name == "category")
		return m_libraryBrowserContextCategory;
	else if (name == "id")
		return m_libraryBrowserContextId;
	else if (name == "filter")
		return m_libraryBrowserContextFilter;
	else if (name == "search")
		return m_libraryBrowserContextSearch;

	DevMsg("Unhandled Error: Invalid context variable name.\n");
	return "contextError";
}

void C_MetaverseManager::ScaleObject(C_PropShortcutEntity* pEntity, int iDelta)
{
	float goodScale = pEntity->GetModelScale() + 0.1f * iDelta;

//	C_PropShortcutEntity* pEntity = this->GetSpawningObjectEntity();
	engine->ServerCmd(VarArgs("setscale %i %f", pEntity->entindex(), goodScale), false);
	/*
	object_t* object = g_pAnarchyManager->GetInstanceManager()->GetInstanceObject(pEntity->GetObjectId());
	if (object)
	{
		object->scale += 0.1f * iDelta;	// NOTE: Changes are made to the object here but aren't saved yet!! (is this ok?)
		C_PropShortcutEntity* pEntity = this->GetSpawningObjectEntity();
		engine->ServerCmd(VarArgs("setscale %i %f", pEntity->entindex(), object->scale), false);
	}
	*/
}

void C_MetaverseManager::SetObjectScale(C_PropShortcutEntity* pEntity, float scale)
{
	engine->ServerCmd(VarArgs("setscale %i %f", pEntity->entindex(), scale), false);
}

int C_MetaverseManager::CycleSpawningRotationAxis(int direction)
{
	m_iSpawningRotationAxis += direction;
	if (m_iSpawningRotationAxis > 2)
		m_iSpawningRotationAxis = 0;
	else if (m_iSpawningRotationAxis < 0)
		m_iSpawningRotationAxis = 2;

	return m_iSpawningRotationAxis;
}

void C_MetaverseManager::ResetSpawningAngles()
{
	m_spawningAngles.x = 0;
	m_spawningAngles.y = 0;
	m_spawningAngles.z = 0;
}

void C_MetaverseManager::GetObjectInfo(object_t* pObject, KeyValues* &pObjectInfo, KeyValues* &pItemInfo, KeyValues* &pModelInfo)
{
	pObjectInfo = new KeyValues("objectInfo");

	pObjectInfo->SetString("id", pObject->objectId.c_str());
	pObjectInfo->SetBool("slave", pObject->slave);
	pObjectInfo->SetFloat("scale", pObject->scale);

	KeyValues* pItemKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(g_pAnarchyManager->GetMetaverseManager()->GetLibraryItem(pObject->itemId));
	if (pItemKV)
	{
		pItemInfo = new KeyValues("itemInfo");

		pItemInfo->SetString("id", pItemKV->GetString("info/id"));
		pItemInfo->SetString("title", pItemKV->GetString("title"));

		std::string workshopId = pItemKV->GetString(VarArgs("platforms/%s/workshopId", AA_PLATFORM_ID));
		pItemInfo->SetString("workshopIds", workshopId.c_str());

		// if there is a workshop ID, get more info.
		SteamWorkshopDetails_t* pWorkshopDetails = g_pAnarchyManager->GetWorkshopManager()->GetWorkshopSubscription(Q_atoui64(workshopId.c_str()));
		if (pWorkshopDetails)
			pItemInfo->SetString("workshopTitle", pWorkshopDetails->title.c_str());
		else
			pItemInfo->SetString("workshopTitle", "");

		/*
		KeyValues* someInfoKV = pItemKV->FindKey(VarArgs("platforms/%s", AA_PLATFORM_ID));
		if (someInfoKV)
		{
			for (KeyValues *sub = someInfoKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
				DevMsg("Test value: %s\n", sub->GetName());
		}
		*/
		//pItemInfo->SetString("workshopIds", "TBD");
	}

	KeyValues* pModelKV = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(g_pAnarchyManager->GetMetaverseManager()->GetLibraryModel(pObject->modelId));
	if (pModelKV)
	{
		pModelInfo = new KeyValues("modelInfo");

		pModelInfo->SetString("id", pModelKV->GetString("info/id"));
		pModelInfo->SetString("title", pModelKV->GetString("title"));
		
		KeyValues* someInfoKV = g_pAnarchyManager->GetMetaverseManager()->DetectRequiredWorkshopForModelFile(pModelKV->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID)));
		std::string workshopTitle = "";
		for (KeyValues *sub = someInfoKV->GetFirstSubKey(); sub; sub = sub->GetNextKey())
			pModelInfo->SetString(sub->GetName(), workshopTitle.c_str());

		std::string workshopId = someInfoKV->GetString("workshopIds");
		someInfoKV->deleteThis();
		SteamWorkshopDetails_t* pWorkshopDetails = g_pAnarchyManager->GetWorkshopManager()->GetWorkshopSubscription(Q_atoui64(workshopId.c_str()));
		pModelInfo->SetString("workshopIds", workshopId.c_str());

		if (pWorkshopDetails)
			pModelInfo->SetString("workshopTitle", pWorkshopDetails->title.c_str());
		else
			pModelInfo->SetString("workshopTitle", "");

		// backpack stuff
		std::string backpackTitle;
		std::string backpackId = g_pAnarchyManager->GetBackpackManager()->DetectRequiredBackpackForModelFile(pModelKV->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID)));
		if (backpackId != "")
		{
			C_Backpack* pBackpack = g_pAnarchyManager->GetBackpackManager()->GetBackpack(backpackId);
			backpackTitle = pBackpack->GetTitle();
		}

		pModelInfo->SetString("backpackIds", backpackId.c_str());
		pModelInfo->SetString("backpackTitle", backpackTitle.c_str());

		/*
		std::string workshopId = pModelKV->GetString(VarArgs("platforms/%s/workshopId", AA_PLATFORM_ID));
		pModelInfo->SetString("workshopIds", workshopId.c_str());

		// if there is a workshop ID, get more info.
		SteamWorkshopDetails_t* pWorkshopDetails = g_pAnarchyManager->GetWorkshopManager()->GetWorkshopSubscription(Q_atoui64(workshopId.c_str()));
		if (pWorkshopDetails)
			pModelInfo->SetString("workshopTitle", pWorkshopDetails->title.c_str());
		else
			pModelInfo->SetString("workshopTitle", "");*/

		//pModelInfo->SetString("mountIds", "TBD");
		
		//pModelInfo->SetString("file", pModelKV->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID)));
	}
}

KeyValues* C_MetaverseManager::DetectRequiredWorkshopForMapFile(std::string mapFile)
{
	// BEST CAST: this map already has an entry in the library
	// TODO: work

	// else, WORST CASE: figure out where the map is from based on its file location
	char* fullPath = new char[AA_MAX_STRING];
	PathTypeQuery_t pathTypeQuery;
	//std::string fullPathBuf;

	std::string mapBuf = mapFile;
	
	size_t found = mapBuf.find_last_of("/\\");
	if (found != std::string::npos)
		mapBuf = mapBuf.substr(found + 1);

	mapBuf = std::string("maps/") + mapBuf;

	g_pFullFileSystem->RelativePathToFullPath(mapBuf.c_str(), "GAME", fullPath, AA_MAX_STRING, FILTER_NONE, &pathTypeQuery);
	mapBuf = fullPath;

	// mapBuf is now a full file path to the BSP
	std::string workshopId;
	std::string mountId;
	std::string mountTitle;
	bool bIsWorkshop = false;
	bool bIsLegacyWorkshop = false;
	bool bIsLegacyImported = false;
	bool bIsLegacyImportedGen1Workshop = false;	// legacy imported GEN 1 workshop maps get treated like regular legacy imported non-workshop maps

	std::string baseDir = engine->GetGameDirectory();
	std::string importedLegacyDir = g_pAnarchyManager->GetLegacyFolder();
	std::string workshopDir = g_pAnarchyManager->GetWorkshopFolder();

	// Source gives the map path in all-lowercase (WHY GABE N?!) so the paths we test against also need to be lowercase
	std::transform(baseDir.begin(), baseDir.end(), baseDir.begin(), ::tolower);
	std::transform(importedLegacyDir.begin(), importedLegacyDir.end(), importedLegacyDir.begin(), ::tolower);
	std::transform(workshopDir.begin(), workshopDir.end(), workshopDir.begin(), ::tolower);

	if (importedLegacyDir != "" && importedLegacyDir.find_last_of("\\") == importedLegacyDir.length() - 1)
	{
		// check for the very old GEN1 workshop maps being imported from a legacy folder.  importing these are not supported (but subscribing to them is)
		std::string legacyWorkshopGen1MapsDir = importedLegacyDir + "workshop\\workshopmaps\\maps\\";

		if (mapBuf.find(legacyWorkshopGen1MapsDir) == 0)
			bIsLegacyImportedGen1Workshop = true;
	}
	
	if (!bIsLegacyImportedGen1Workshop)
	{
		//DevMsg("Map: %s vs %s\n", mapBuf.c_str(), workshopDir.c_str());
		// check for content from the workshop
		if (mapBuf.find(workshopDir) == 0)
		{
			bIsWorkshop = true;
			// extract the workshop ID
			workshopId = mapBuf.substr(workshopDir.length());
			workshopId = workshopId.substr(0, workshopId.find_last_of("\\"));
			workshopId = workshopId.substr(0, workshopId.find_last_of("\\"));

			// TODO: determine if this is a legacy workshop map (somehow)
			//bIsLegacyWorkshop = ?;
		}
	}

	C_Mount* pMount = g_pAnarchyManager->GetMountManager()->FindOwningMount(mapBuf);
	if (pMount)
	{
		//	DevMsg("Yar.\n");
		mountId = pMount->GetId();
		mountTitle = pMount->GetTitle();
	}

	// populate mapInfo with stuff
	KeyValues* mapInfo = new KeyValues("mapInfo");
	mapInfo->SetString("fullfile", mapBuf.c_str());
	mapInfo->SetString("workshopIds", workshopId.c_str());
	mapInfo->SetString("mountIds", mountId.c_str());
	mapInfo->SetString("mountTitle", mountTitle.c_str());
	mapInfo->SetBool("bIsWorkshop", bIsWorkshop);
	mapInfo->SetBool("bIsLegacyImportedGen1Workshop", bIsLegacyImportedGen1Workshop);
	return mapInfo;
}

KeyValues* C_MetaverseManager::DetectRequiredWorkshopForModelFile(std::string modelFile)
{
	// BEST CAST: this model already has an entry in the library
	// TODO: work

	// else, WORST CASE: figure out where the map is from based on its file location
	char* fullPath = new char[AA_MAX_STRING];
	PathTypeQuery_t pathTypeQuery;
	//std::string fullPathBuf;

	std::string modelBuf = modelFile;

	/*
	size_t found = modelBuf.find_last_of("/\\");
	if (found != std::string::npos)
		modelBuf = modelBuf.substr(found + 1);

	modelBuf = std::string("maps/") + modelBuf;
	*/
	DevMsg("Here model is: %s\n", modelBuf.c_str());
	g_pFullFileSystem->RelativePathToFullPath(modelBuf.c_str(), "GAME", fullPath, AA_MAX_STRING, FILTER_NONE, &pathTypeQuery);
	modelBuf = fullPath;
	DevMsg("And now it is: %s\n", modelBuf.c_str());

	// modelBuf is now a full file path to the BSP
	std::string workshopId;
	std::string mountId;
	std::string mountTitle;
	bool bIsWorkshop = false;
	bool bIsLegacyImported = false;
	//bool bIsLegacyWorkshop = false;

	std::string baseDir = engine->GetGameDirectory();
	std::string importedLegacyDir = g_pAnarchyManager->GetLegacyFolder();
	std::string workshopDir = g_pAnarchyManager->GetWorkshopFolder();

	// Source gives the model path in all-lowercase (WHY GABE N?!) so the paths we test against also need to be lowercase
	std::transform(baseDir.begin(), baseDir.end(), baseDir.begin(), ::tolower);
	std::transform(importedLegacyDir.begin(), importedLegacyDir.end(), importedLegacyDir.begin(), ::tolower);
	std::transform(workshopDir.begin(), workshopDir.end(), workshopDir.begin(), ::tolower);

	if (modelBuf.find(importedLegacyDir) == 0)
		bIsLegacyImported = true;

	// check for content from the workshop
	if (modelBuf.find(workshopDir) == 0)
	{
		bIsWorkshop = true;

		// extract the workshop ID
		workshopId = modelBuf.substr(workshopDir.length());
		workshopId = workshopId.substr(0, workshopId.find("\\"));

		// TODO: determine if this is a legacy workshop model (somehow)
		//bIsLegacyWorkshop = ?;
	}

	// determine mount ID

	C_Mount* pMount = g_pAnarchyManager->GetMountManager()->FindOwningMount(modelBuf);
	if (pMount)
	{
	//	DevMsg("Yar.\n");
		mountId = pMount->GetId();
		mountTitle = pMount->GetTitle();
	}

	// populate mapInfo with stuff
	KeyValues* modelInfo = new KeyValues("modelInfo");
	modelInfo->SetString("fullfile", modelBuf.c_str());
	modelInfo->SetString("workshopIds", workshopId.c_str());
	modelInfo->SetString("mountIds", mountId.c_str());
	modelInfo->SetString("mountTitle", mountTitle.c_str());	// FIXME: This is singluar while the other ones are plural.  Hmmmmm.
	modelInfo->SetBool("bIsWorkshop", bIsWorkshop);
	modelInfo->SetBool("bIsLegacyImported", bIsLegacyImported);
	return modelInfo;
}

void C_MetaverseManager::FlagDynamicModels()
{
	// technically shouldn't be needed, but it does allow addon cabinet models to be added to the list just by living in the right folder, and thats cool.
	std::string buf;
	KeyValues* active;
	std::map<std::string, KeyValues*>::iterator it = m_models.begin();
	while (it != m_models.end())
	{
		active = this->GetActiveKeyValues(it->second);
		//active = it->second->FindKey("current");
		//if (!active)
		//	active = it->second->FindKey("local", true);

		if (active->GetInt("dynamic") != 1)
		{
			buf = active->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID));
			if ((buf.find("models\\cabinets\\") == 0 || buf.find("models/cabinets/") == 0 || buf.find("models\\banners\\") == 0 || buf.find("models/banners/") == 0 || buf.find("models\\frames\\") == 0 || buf.find("models/frames/") == 0 || buf.find("models\\icons\\") == 0 || buf.find("models/icons/") == 0) && (buf.find("room_divider.mdl") == std::string::npos && buf.find("newton_toy.mdl") == std::string::npos))
			{
				active->SetInt("dynamic", 1);
				DevMsg("Flagged %s as dynamic model\n", buf.c_str());
			}
		}
		/*
		else
		{
			buf = active->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID));
			if ()// || (buf.find("models\\cabinets\\") != 0 && buf.find("models/cabinets/") != 0 && buf.find("models\\banners\\") != 0 && buf.find("models/banners/") != 0 && buf.find("models\\frames\\") != 0 && buf.find("models/frames/") != 0 && buf.find("models\\icons\\") != 0 && buf.find("models/icons/") != 0))
			{
				active->SetInt("dynamic", 0);
				DevMsg("Un-flagged %s as dynamic model\n", buf.c_str());
			}
		}
		*/

		it++;
	}
}

KeyValues* C_MetaverseManager::DetectFirstMap(bool& bAlreadyExists)
{
	//if (m_previousDetectLocalMapFilePath != "")	// FIXME: need a way to detect if there is already a DetectFirst/Next query active.
		//this->DetectLocalMapClose();

	instance_t* pInstance;
	const char *pFilename = g_pFullFileSystem->FindFirstEx("maps\\*.bsp", "GAME", &m_previousDetectLocalMapFileHandle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(m_previousDetectLocalMapFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(m_previousDetectLocalMapFileHandle);
			continue;
		}

		std::string foundName = pFilename;
		//foundName = VarArgs("maps\\%s", pFilename);

		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);
		foundName = path_buffer;
		// FINISHED MAKING THE FILE PATH NICE

		KeyValues* active;
		std::map<std::string, KeyValues*>::iterator it = m_maps.begin();
		while (it != m_maps.end())
		{
			active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(it->second);

			if (!Q_strcmp(active->GetString("platforms/-KJvcne3IKMZQTaG7lPo/file"), foundName.c_str()))
			{
				//g_pFullFileSystem->FindClose(m_previousDetectLocalMapFileHandle);
				bAlreadyExists = true;
				return it->second;
			}

			it++;
		}

		// if we haven't found a map for this yet, let's make one.
		KeyValues* map = new KeyValues("map");
		map->SetInt("generation", 3);

		// add standard info
		//std::string id = g_pAnarchyManager->GenerateUniqueId();

		std::string goodTitle = pFilename;
		size_t found = goodTitle.find(".");
		if (found != std::string::npos)
			goodTitle = goodTitle.substr(0, found);

		std::string id = g_pAnarchyManager->GenerateLegacyHash(goodTitle.c_str());

		map->SetString("local/info/id", id.c_str());
		map->SetInt("local/info/created", 0);
		map->SetString("local/info/owner", "local");
		map->SetInt("local/info/removed", 0);
		map->SetString("local/info/remover", "");
		map->SetString("local/info/alias", "");

		std::string mapName = foundName.substr(0, foundName.length() - 4);
		map->SetString("local/title", mapName.c_str());
		map->SetString("local/keywords", "");
		map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/file", foundName.c_str());

		KeyValues* stuffKV = g_pAnarchyManager->GetMetaverseManager()->DetectRequiredWorkshopForMapFile(foundName.c_str());
		if (stuffKV)
		{
			map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/workshopIds", stuffKV->GetString("workshopIds"));
			map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/mountIds", stuffKV->GetString("mountIds"));
			stuffKV->deleteThis();
		}

		m_maps[id.c_str()] = map;
		bAlreadyExists = false;

		// update any legacy instances that were detected that use this map
		std::string legacyMapName = foundName.substr(0, foundName.length() - 4);
		DevMsg("LEGACY MAP NAME: %s\n", legacyMapName.c_str());
		g_pAnarchyManager->GetInstanceManager()->LegacyMapIdFix(legacyMapName, id);
//		if (pInstance)
	//		pInstance->mapId = id;

		return map;
	}

	g_pFullFileSystem->FindClose(m_previousDetectLocalMapFileHandle);
	return null;
}

KeyValues* C_MetaverseManager::DetectNextMap(bool& bAlreadyExists)
{
	instance_t* pInstance;
	const char *pFilename = g_pFullFileSystem->FindNext(m_previousDetectLocalMapFileHandle);
	while (pFilename != NULL)
	{
		if (g_pFullFileSystem->FindIsDirectory(m_previousDetectLocalMapFileHandle))
		{
			pFilename = g_pFullFileSystem->FindNext(m_previousDetectLocalMapFileHandle);
			continue;
		}

		std::string foundName = pFilename;
		//foundName = VarArgs("maps\\%s", pFilename);

		// MAKE THE FILE PATH NICE
		char path_buffer[AA_MAX_STRING];
		Q_strcpy(path_buffer, foundName.c_str());
		V_FixSlashes(path_buffer);
		foundName = path_buffer;
		// FINISHED MAKING THE FILE PATH NICE

		KeyValues* active;
		std::map<std::string, KeyValues*>::iterator it = m_maps.begin();
		while (it != m_maps.end())
		{
			active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(it->second);

			if (!Q_strcmp(active->GetString("platforms/-KJvcne3IKMZQTaG7lPo/file"), foundName.c_str()))
			{
				//g_pFullFileSystem->FindClose(m_previousDetectLocalMapFileHandle);
				bAlreadyExists = true;
				return it->second;
			}

			it++;
		}

		// if we haven't found a map for this yet, let's make one.
		KeyValues* map = new KeyValues("map");
		map->SetInt("generation", 3);

		// add standard info
		//std::string id = g_pAnarchyManager->GenerateUniqueId();

		std::string goodTitle = pFilename;
		size_t found = goodTitle.find(".");
		if (found != std::string::npos)
			goodTitle = goodTitle.substr(0, found);

		//DevMsg("Generating hash based on: %s\n", goodTitle.c_str());
		std::string id = g_pAnarchyManager->GenerateLegacyHash(goodTitle.c_str());

		map->SetString("local/info/id", id.c_str());
		map->SetInt("local/info/created", 0);
		map->SetString("local/info/owner", "local");
		map->SetInt("local/info/removed", 0);
		map->SetString("local/info/remover", "");
		map->SetString("local/info/alias", "");

		std::string mapName = foundName.substr(0, foundName.length() - 4);
		map->SetString("local/title", mapName.c_str());
		map->SetString("local/keywords", "");
		map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/file", foundName.c_str());

		KeyValues* stuffKV = g_pAnarchyManager->GetMetaverseManager()->DetectRequiredWorkshopForMapFile(foundName.c_str());
		if (stuffKV)
		{
			map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/workshopIds", stuffKV->GetString("workshopIds"));
			map->SetString("local/platforms/-KJvcne3IKMZQTaG7lPo/mountIds", stuffKV->GetString("mountIds"));
			stuffKV->deleteThis();
		}

		m_maps[id.c_str()] = map;
		bAlreadyExists = false;

		// update any legacy instances that were detected that use this map
		std::string legacyMapName = foundName.substr(0, foundName.length() - 4);
		g_pAnarchyManager->GetInstanceManager()->LegacyMapIdFix(legacyMapName, id);
//		pInstance = g_pAnarchyManager->GetInstanceManager()->FindInstance(foundName.substr(0, foundName.length() - 4));
	//	if (pInstance)
		//	pInstance->mapId = id;

		return map;
	}

	g_pFullFileSystem->FindClose(m_previousDetectLocalMapFileHandle);
	return null;
}

// Legacy, because it has a file path.
KeyValues* C_MetaverseManager::LoadLocalItem(std::string file, std::string filePath)
{

	//DevMsg("Attemping to load item %s from %s\n", file.c_str(), filePath.c_str());
	KeyValues* pItem = new KeyValues("item");
	bool bLoaded;

	if (filePath != "")
	{
		std::string fullFile = filePath + file;
		bLoaded = pItem->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), "");
	}
	else
		bLoaded = pItem->LoadFromFile(g_pFullFileSystem, file.c_str(), "MOD");

	if (!bLoaded)
	{
		pItem->deleteThis();
		pItem = null;
	}
	else
	{
		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pItem);

		std::string id = pActive->GetString("info/id");

		std::vector<std::string>* pDefaultFields = g_pAnarchyManager->GetMetaverseManager()->GetDefaultFields();

		unsigned int max = (*pDefaultFields).size();
		for (unsigned int i = 0; i < max; i++)
		{
			if (!pActive->FindKey((*pDefaultFields)[i].c_str()))
				pActive->SetString((*pDefaultFields)[i].c_str(), "");
		}

		if (!pActive->FindKey("type") || !Q_strcmp(pActive->GetString("type"), ""))
			pActive->SetString("type", AA_DEFAULT_TYPEID);
		
		//DevMsg("Finished loading item\n");

		auto it = m_items.find(id);
		if (it != m_items.end())
		{
			// FIXME: Merg with existing item (keeping in mind that non-legacy items overpower legacy items
			if (it->second->GetBool("loadedFromLegacy"))
			{
				this->SmartMergItemKVs(pItem, it->second);
				it->second->deleteThis();
				it->second = pItem;

				// Then remove the loadedFromLegacy tag, if it exits.
				KeyValues* pTargetKey = pItem->FindKey("loadedFromLegacy");
				if (pTargetKey)
					pItem->RemoveSubKey(pTargetKey);
			}
			else
			{
				this->SmartMergItemKVs(it->second, pItem);
				pItem->deleteThis();
				pItem = it->second;
			}
		}
		else
			m_items[id] = pItem;
	}

	return pItem;
}

unsigned int C_MetaverseManager::LoadAllLocalItems(sqlite3* pDb, std::map<std::string, KeyValues*>* pResponseMap)
{
	if (!pDb)
		pDb = m_db;

	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from items", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	int length;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
			continue;
		}

		KeyValues* pItem = new KeyValues("item");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
		pItem->ReadAsBinary(buf);
		buf.Purge();

		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pItem);

		count++;

		std::string id = pActive->GetString("info/id");

		std::vector<std::string>* pDefaultFields = g_pAnarchyManager->GetMetaverseManager()->GetDefaultFields();

		unsigned int max = (*pDefaultFields).size();
		for (unsigned int i = 0; i < max; i++)
		{
			if (!pActive->FindKey((*pDefaultFields)[i].c_str()))
				pActive->SetString((*pDefaultFields)[i].c_str(), "");
		}

		if (!pActive->FindKey("type") || !Q_strcmp(pActive->GetString("type"), ""))
			pActive->SetString("type", AA_DEFAULT_TYPEID);

		if (pResponseMap)
			(*pResponseMap)[id] = pItem;
		else
			m_items[id] = pItem;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.
	return count;
}

KeyValues* C_MetaverseManager::LoadLocalModel(std::string file, std::string filePath)
{
	// make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	KeyValues* pModel = new KeyValues("model");
	bool bLoaded;

	if (filePath != "")
	{
		std::string fullFile = filePath + file;
		bLoaded = pModel->LoadFromFile(g_pFullFileSystem, fullFile.c_str(), "");
	}
	else
		bLoaded = pModel->LoadFromFile(g_pFullFileSystem, file.c_str(), "MOD");

	if (!bLoaded)
	{
		pModel->deleteThis();
		pModel = null;
	}
	else
	{
		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pModel);

		std::string id = pActive->GetString("info/id");

		std::vector<std::string> defaultFields;	// Should platform specific fields be expected as well?  Jump off that bridge when you cross it.
		defaultFields.push_back("title");
		defaultFields.push_back("keywords");
		defaultFields.push_back("dynamic");

		unsigned int max = defaultFields.size();
		for (unsigned int i = 0; i < max; i++)
		{
			if (!pActive->FindKey(defaultFields[i].c_str()))
				pActive->SetString(defaultFields[i].c_str(), "");
		}

		m_models[id] = pModel;
	}

	return pModel;
}

unsigned int C_MetaverseManager::LoadAllLocalModels(unsigned int& numDynamic, sqlite3* pDb, std::map<std::string, KeyValues*>* pResponseMap)
{
	if (!pDb)
		pDb = m_db;

	// make it use the new shinnit
	unsigned int count = 0;
	sqlite3_stmt *stmt = NULL;
	int rc = sqlite3_prepare(pDb, "SELECT * from models", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		DevMsg("prepare failed: %s\n", sqlite3_errmsg(pDb));

	int length;
	while (sqlite3_step(stmt) == SQLITE_ROW)	// THIS IS WHERE THE LOOP CAN BE BROKEN UP AT!!
	{
		length = sqlite3_column_bytes(stmt, 1);

		if (length == 0)
		{
			DevMsg("WARNING: Zero-byte KeyValues skipped.\n");
			continue;
		}

		KeyValues* pModel = new KeyValues("model");

		CUtlBuffer buf(0, length, 0);
		buf.CopyBuffer(sqlite3_column_blob(stmt, 1), length);
		pModel->ReadAsBinary(buf);
		buf.Purge();

		// TODO: Look up any alias here first!!
		KeyValues* pActive = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(pModel);

		count++;

		std::string id = pActive->GetString("info/id");

		std::vector<std::string> defaultFields;	// Should platform specific fields be expected as well?  Jump off that bridge when you cross it.
		defaultFields.push_back("title");
		defaultFields.push_back("keywords");
		defaultFields.push_back("dynamic");

		unsigned int max = defaultFields.size();
		for (unsigned int i = 0; i < max; i++)
		{
			if (!pActive->FindKey(defaultFields[i].c_str()))
				pActive->SetString(defaultFields[i].c_str(), "");
		}


		if (pResponseMap)
			(*pResponseMap)[id] = pModel;
		else
			m_models[id] = pModel;

		if (pActive->GetBool("dynamic"))
			numDynamic++;
	}
	sqlite3_finalize(stmt);	// TODO: error checking?  Maybe not needed, if this is like a close() operation.
	return count;
}

std::string C_MetaverseManager::ResolveLegacyModel(std::string legacyModel)
{
	// iterate through the models
	KeyValues* active;
	for (std::map<std::string, KeyValues*>::iterator it = m_models.begin(); it != m_models.end(); ++it)
	{
		active = g_pAnarchyManager->GetMetaverseManager()->GetActiveKeyValues(it->second);

		// MAKE THE FILE PATH NICE
		char niceModel[AA_MAX_STRING];
		Q_strcpy(niceModel, legacyModel.c_str());
		V_FixSlashes(niceModel);

		//for (int i = 0; niceModel[i] != '\0'; i++)
		//	niceModel[i] = tolower(niceModel[i]);
		// FINISHED MAKING THE FILE PATH NICE

		if (!Q_stricmp(active->GetString(VarArgs("platforms/%s/file", AA_PLATFORM_ID)), niceModel))
		{
			return it->first;
		}
	}

	return "";
}