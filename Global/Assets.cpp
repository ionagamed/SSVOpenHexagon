// Copyright (c) 2013 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <json/json.h>
#include <json/reader.h>
#include "Data/MusicData.h"
#include "Data/LevelData.h"
#include "Data/EventData.h"
#include "Data/PackData.h"
#include "Data/ProfileData.h"
#include "Data/StyleData.h"
#include "Global/Assets.h"
#include "Utils/Utils.h"

using namespace std;
using namespace sf;
using namespace ssvs;
using namespace ssvs::Utils;
using namespace ssvs::FileSystem;

namespace hg
{
	AssetManager assetManager;
	map<string, Music*> musicPtrsMap;
	map<string, MusicData> musicDataMap;
	map<string, StyleData> styleDataMap;
	map<string, LevelData> levelDataMap;
	map<string, ProfileData> profileDataMap;
	map<string, EventData> eventDataMap;
	map<string, PackData> packDataMap;
	ProfileData* currentProfilePtr{nullptr};
	map<string, vector<string>> levelIdsByPackMap;
	vector<string> packPaths;

	void initAssetManager() { assetManager.loadFolder("Assets/"); }
	AssetManager& getAssetManager() { return assetManager; }

	void loadAssets()
	{
		log("loading profiles", "ASSETS"); 	loadProfiles();

		for(string packPath : getFolders("Packs/"))
		{
			string packName{packPath.substr(6, packPath.length() - 6)};

			Json::Value packRoot{getJsonFileRoot(packPath + "/pack.json")};
			PackData packData(packName, packRoot["name"].asString(), packRoot["priority"].asFloat());
			packDataMap.insert(make_pair(packName, packData));
		}

		vector<PackData> packDatasToQuery;
		for(pair<string, PackData> packDataPair : packDataMap) packDatasToQuery.push_back(packDataPair.second);
		sort(begin(packDatasToQuery), end(packDatasToQuery), [](PackData a, PackData b) -> bool { return a.getPriority() < b.getPriority(); });

		for(PackData packData : packDatasToQuery)
		{
			string packName{packData.getId()};
			packPaths.push_back("Packs/" + packName + "/");
			log("loading " + packName + " music", "ASSETS"); 		loadMusic("Packs/" + packName + "/");
			log("loading " + packName + " music data", "ASSETS"); 	loadMusicData("Packs/" + packName + "/");
			log("loading " + packName + " style data", "ASSETS"); 	loadStyleData("Packs/" + packName + "/");
			log("loading " + packName + " level data", "ASSETS");	loadLevelData("Packs/" + packName + "/");
			log("loading " + packName + " events", "ASSETS"); 		loadEvents("Packs/" + packName + "/");
			log("loading " + packName + " custom sounds", "ASSETS");loadCustomSounds(packName, "Packs/" + packName + "/");
		}
	}

	void loadCustomSounds(string mPackName, string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Sounds/", ".ogg"))
		{
			string fileName{getNameFromPath(filePath, mPath + "Sounds/", "")};
			assetManager.loadSound(mPackName + "_" + fileName, filePath);
			assetManager.getSound(mPackName + "_" + fileName).setVolume(getSoundVolume());
		}
	}
	void loadMusic(string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Music/", ".ogg"))
		{
			string fileName{getNameFromPath(filePath, mPath + "Music/", ".ogg")};

			Music* music{new Music};
			music->openFromFile(filePath);
			music->setVolume(getMusicVolume());
			musicPtrsMap.insert(make_pair(fileName, music));
		}
	}
	void loadMusicData(string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Music/", ".json"))
		{
			MusicData musicData{loadMusicFromJson(getJsonFileRoot(filePath))};
			musicDataMap.insert(make_pair(musicData.getId(), musicData));
		}
	}
	void loadStyleData(string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Styles/", ".json"))
		{
			StyleData styleData{loadStyleFromJson(getJsonFileRoot(filePath))};
			styleDataMap.insert(make_pair(styleData.getId(), styleData));
		}
	}
	void loadLevelData(string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Levels/", ".json"))
		{
			LevelData levelData{loadLevelFromJson(getJsonFileRoot(filePath))};
			levelData.setPackPath(mPath);
			levelDataMap.insert(make_pair(levelData.getId(), levelData));
			levelIdsByPackMap[levelData.getPackPath()].push_back(levelData.getId());
		}
	}
	void loadProfiles()
	{
		for(auto filePath : getFilesByExtension("Profiles/", ".json"))
		{
			string fileName{getNameFromPath(filePath, "Profiles/", ".json")};

			ProfileData profileData{loadProfileFromJson(getJsonFileRoot(filePath))};
			profileDataMap.insert(make_pair(profileData.getName(), profileData));
		}
	}
	void loadEvents(string mPath)
	{
		for(auto filePath : getFilesByExtension(mPath + "Events/", ".json"))
		{
			EventData eventData{getJsonFileRoot(filePath)};
			eventDataMap.insert(make_pair(eventData.getId(), eventData));
		}
	}

	void saveCurrentProfile()
	{
		if(currentProfilePtr == nullptr) return;

		Json::StyledStreamWriter writer;
		ofstream o{getCurrentProfileFilePath(), std::ifstream::binary};

		Json::Value profileRoot;
		profileRoot["version"] = getVersion();
		profileRoot["name"] = getCurrentProfile().getName();
		profileRoot["scores"] = getCurrentProfile().getScores();

		writer.write(o, profileRoot);
		o.flush();
		o.close();
	}

	vector<LevelData> getAllLevelData()
	{
		vector<LevelData> result;
		for(auto pair : levelDataMap) result.push_back(pair.second);
		return result;
	}
	vector<string> getAllMenuLevelDataIds()
	{
		vector<LevelData> levelDataVector{getAllLevelData()};
		sort(begin(levelDataVector), end(levelDataVector),
		[](LevelData a, LevelData b) -> bool
		{
			if(a.getPackPath() == b.getPackPath())
				return a.getMenuPriority() < b.getMenuPriority();

			return a.getPackPath() < b.getPackPath();
		});

		vector<string> result;
		for(auto levelData : levelDataVector) if(levelData.getSelectable()) result.push_back(levelData.getId());
		return result;
	}
	vector<string> getMenuLevelDataIdsByPack(string mPackPath)
	{
		vector<LevelData> levelDataVector;
		for(string id : levelIdsByPackMap[mPackPath]) levelDataVector.push_back(getLevelData(id));

		sort(begin(levelDataVector), end(levelDataVector),
		[](LevelData a, LevelData b) -> bool
		{
			return a.getMenuPriority() < b.getMenuPriority();
		});

		vector<string> result;
		for(auto levelData : levelDataVector) if(levelData.getSelectable()) result.push_back(levelData.getId());
		return result;
	}
	vector<string> getPackPaths() { return packPaths; }
	vector<string> getPackNames()
	{
		vector<string> result;
		for(auto& packPair : packDataMap) result.push_back(packPair.first);
		return result;
	}

	void stopAllMusic() { for(auto pair : musicPtrsMap) pair.second->stop(); }
	void stopAllSounds() { assetManager.stopSounds(); }
	void playSound(string mId) { if(!getNoSound()) getSoundPtr(mId)->play(); }

	Font& getFont(string mId) 				{ return assetManager.getFont(mId); }
	Sound* getSoundPtr(string mId) 			{ return &assetManager.getSound(mId); }
	Music* getMusicPtr(string mId) 			{ return musicPtrsMap.find(mId)->second; }
	MusicData getMusicData(string mId) 		{ return musicDataMap.find(mId)->second; }
	StyleData getStyleData(string mId) 		{ return styleDataMap.find(mId)->second; }
	LevelData getLevelData(string mId) 		{ return levelDataMap.find(mId)->second; }
	PackData getPackData(string mId) 		{ return packDataMap.find(mId)->second; }

	float getScore(string mId) 				{ return getCurrentProfile().getScore(mId); }
	void setScore(string mId, float mScore) { getCurrentProfile().setScore(mId, mScore); }

	void setCurrentProfile(string mName) { currentProfilePtr = &profileDataMap.find(mName)->second; }
	ProfileData& getCurrentProfile() { return *currentProfilePtr; }
	string getCurrentProfileFilePath() { return "Profiles/" + currentProfilePtr->getName() + ".json"; }
	void createProfile(string mName)
	{
		ofstream o{"Profiles/" + mName + ".json"};
		Json::Value root;
		Json::StyledStreamWriter writer;

		root["name"] = mName;
		root["scores"] = Json::objectValue;
		writer.write(o, root);
		o.flush();
		o.close();

		profileDataMap.clear();
		loadProfiles();
	}
	int getProfilesSize() { return profileDataMap.size(); }
	vector<string> getProfileNames()
	{
		vector<string> result;
		for(auto pair : profileDataMap) result.push_back(pair.second.getName());
		return result;
	}
	string getFirstProfileName() { return profileDataMap.begin()->second.getName(); }

	EventData* getEventData(string mId, HexagonGame* mHgPtr)
	{
		EventData* result = new EventData(eventDataMap.find(mId)->second);
		result->setHexagonGamePtr(mHgPtr);
		return result;
	}
}
