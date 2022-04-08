#ifndef ATTRIBUTESLOADER_H
#define ATTRIBUTESLOADER_H
#ifdef _WIN32
#pragma once
#endif

class CAttributesLoader
{
public:
	CAttributesLoader(const char* className, int preset, bool noError = false);
	virtual void Init(const char* className, int preset, bool noError = false);
	virtual const char* GetString(const char* szString, const char* defaultValue = "");
	virtual int GetInt(const char* szString, int defaultValue = 0);
	virtual float GetFloat(const char* szString, float defaultValue = 0.0f);
	virtual bool GetBool(const char* szString, bool defaultValue = false);
	virtual Color GetColor(const char* szString);
	virtual Vector GetVector(const char* szString, Vector defaultValue = Vector(0,0,0));
	virtual void SwitchEntityModel(CBaseEntity *ent, const char* szString, const char* defaultValue);
	virtual void SwitchEntityColor(CBaseEntity* ent, const char* szString);
	virtual void Die(void);
	
public:
	bool loadedAttributes;

private:
	KeyValues* data;
};

extern ConVar entity_attributes_numpresets;
extern ConVar entity_attributes_chance;
extern ConVar entity_attributes;
CAttributesLoader *LoadRandomPresetFile(const char* className, bool noNag = false);
CAttributesLoader *LoadPresetFile(const char* className, int preset);
#endif