#include "plugin.h"
#include "CTxdStore.h"
#include "CHud.h"

using namespace plugin;

#ifdef GTAVC
// ============================================================================
//  GTA Vice City version
//
//  Vice City keeps one HUD sprite per weapon in CHud::Sprites[] (HUD_FIST ...
//  HUD_CAMERA). This plugin loads "weaponicons.txd" and, for every weapon
//  sprite, swaps in the texture from that file that has the SAME NAME as the
//  icon the game loaded. Icons that are not in weaponicons.txd stay as they are.
//
//  Put weaponicons.txd in the game folder (or in the models folder).
//  To find the icon names the game uses, create an empty file called
//  WeaponIconsTXD.log in the game folder: the plugin then writes every icon
//  name it sees into that file.
// ============================================================================

#include <cstdio>
#include <cstdarg>

class WeaponIconsTXD
{
public:
    inline static RwTexDictionary* customDict = nullptr;
    inline static bool loadTried = false;
    inline static bool logEnabled = false;

    // The texture each weapon sprite held the last time we looked at it.
    inline static RwTexture* lastSeen[HUD_CAMERA + 1] = {};

    WeaponIconsTXD()
    {
        // Runs once per game frame. The check inside is only a few pointer compares,
        // and it also copes with the HUD being re-created (new game, load game).
        Events::gameProcessEvent += []
        {
            Apply();
        };
    }

    static void Log(const char* fmt, ...)
    {
        if (!logEnabled)
            return;
        FILE* f = fopen("WeaponIconsTXD.log", "a");
        if (!f)
            return;
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fputc('\n', f);
        fclose(f);
    }

    static bool FileExists(const char* path)
    {
        FILE* f = fopen(path, "rb");
        if (!f)
            return false;
        fclose(f);
        return true;
    }

    static void LoadCustomTxd()
    {
        loadTried = true;
        logEnabled = FileExists("WeaponIconsTXD.log");

        const char* path = nullptr;
        if (FileExists("weaponicons.txd"))
            path = "weaponicons.txd";
        else if (FileExists("models\\weaponicons.txd"))
            path = "models\\weaponicons.txd";

        if (!path)
        {
            Log("weaponicons.txd not found (looked in the game folder and in models)");
            return;
        }

        int slot = CTxdStore::AddTxdSlot("weaponicons");
        if (!CTxdStore::LoadTxd(slot, path))
        {
            Log("Could not load %s", path);
            CTxdStore::RemoveTxdSlot(slot);
            return;
        }

        CTxdStore::AddRef(slot);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(slot);
        customDict = RwTexDictionaryGetCurrent();
        CTxdStore::PopCurrentTxd();
        Log("Loaded %s", path);
    }

    static void Apply()
    {
        if (!loadTried)
            LoadCustomTxd();

        for (int i = HUD_FIST; i <= HUD_CAMERA; i++)
        {
            RwTexture* cur = CHud::Sprites[i].m_pTexture;
            if (cur == lastSeen[i])
                continue; // nothing new in this slot

            lastSeen[i] = cur;
            if (!cur)
                continue;

            if (!customDict)
            {
                Log("sprite %d uses texture '%s'", i, cur->name);
                continue;
            }

            RwTexture* custom = RwTexDictionaryFindNamedTexture(customDict, cur->name);
            if (!custom)
            {
                Log("sprite %d: '%s' is not in weaponicons.txd, left unchanged", i, cur->name);
                continue;
            }

            Log("sprite %d: replaced '%s'", i, cur->name);
            custom->refCount++;                    // the sprite now owns one reference
            CHud::Sprites[i].m_pTexture = custom;
            RwTextureDestroy(cur);                 // let go of the original, like CSprite2d::Delete does
            lastSeen[i] = custom;
        }
    }

} weaponIconsTXD;

#else
// ============================================================================
//  GTA San Andreas version (original code)
// ============================================================================

#include "CKeyGen.h"

class WeaponIconsTXD
{
public:

    inline static RwTexDictionary* weaponIconsDict;

    WeaponIconsTXD()
    {
        Events::initScriptsEvent += []
        {
            int txdSlot = CTxdStore::AddTxdSlot("weaponicons");
            CTxdStore::LoadTxd(txdSlot, "weaponicons.txd");
            CTxdStore::AddRef(txdSlot);
            CTxdStore::AddRef(txdSlot);
            CTxdStore::SetCurrentTxd(txdSlot);
            weaponIconsDict = plugin::CallAndReturn<RwTexDictionary*, 0x408340, int>(txdSlot);

            if (weaponIconsDict)
            {
                // Check fist icon exists
                if (RwTexDictionaryFindHashNamedTexture(weaponIconsDict, CKeyGen::GetUppercaseKey("fist")))
                {
                    // Instead of m_pTexture, call SetTexture (safer)
                    CHud::Sprites[0].SetTexture((char*)"fist");
                }

                // Patch weapon icons call
                patch::RedirectCall(0x58D84B, CustomRwTexDictionaryFindHashNamedTexture);
                patch::RedirectCall(0x58E895, CustomRwTexDictionaryFindHashNamedTexture);
            }

            CTxdStore::PopCurrentTxd();
        };
    }

    static RwTexture* __cdecl CustomRwTexDictionaryFindHashNamedTexture(RwTexDictionary* a1, int hash)
    {
        RwTexture* retTexture = nullptr;
        retTexture = RwTexDictionaryFindHashNamedTexture(weaponIconsDict, hash);
        if (!retTexture)
        {
            retTexture = RwTexDictionaryFindHashNamedTexture(a1, hash);
        }
        return retTexture;
    }

} weaponIconsTXD;

#endif
