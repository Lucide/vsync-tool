#ifdef __CLION_IDE__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
	#pragma ide diagnostic ignored "misc-misplaced-const"
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <locale.h>
#include <windows.h>
#include <io.h>
// #include <stringapiset.h>
#include "nvapi.h"
#include "NvApiDriverSettings.h"

#define ERROR_HEADER "error: "
#define eprintf(STR, ...) fprintf(stderr, ERROR_HEADER STR "\n", ##__VA_ARGS__)
#define BACKUP_FILENAME L"settings"
int numStaticArgs = 1;

typedef enum Mode {
	MODE_SET = 1,
	MODE_FORCE_SET = 3,
	MODE_RESTORE = 4,
	MODE_HELP = 8
} Mode;

typedef struct VSyncTriplet {
	NVDRS_SETTING vSync, vSyncTearControl, AFR;
} VSyncTriplet;

#define MAGIC_NVDRS_SETTING_VER MAXUINT32 // ugly

VSyncTriplet const vSyncOn = {{.version = MAGIC_NVDRS_SETTING_VER, .settingId = (NvU32)VSYNCMODE_ID, .settingType = NVDRS_DWORD_TYPE, .u32CurrentValue = (NvU32)VSYNCMODE_FORCEON},
                              {.version = MAGIC_NVDRS_SETTING_VER, .settingId = (NvU32)VSYNCTEARCONTROL_ID, .settingType = NVDRS_DWORD_TYPE, .u32CurrentValue = (NvU32)VSYNCTEARCONTROL_DISABLE},
                              {.version = MAGIC_NVDRS_SETTING_VER, .settingId = (NvU32)VSYNCSMOOTHAFR_ID, .settingType = NVDRS_DWORD_TYPE, .u32CurrentValue = (NvU32)VSYNCSMOOTHAFR_OFF}};

char const *UTF16ToUTF8(WCHAR const *const wstr) {
	static char *str;
	if(str) {
		free(str);
	}
	int const strLength = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	str = malloc((unsigned)strLength);
	if(!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, str, strLength, NULL, NULL)) {
		eprintf("string conversion error");
		exit(-1);
	}
	return str;
}

void nvapiExceptionFatal(NvAPI_Status status) {
	if(status != NVAPI_OK) {
		NvAPI_ShortString desc = {0};
		NvAPI_GetErrorMessage(status, desc);
		fprintf(stderr, "NVAPI error: %s\n", desc);
		exit(-abs(status));
	}
}

bool nvapiExceptionPrint(NvAPI_Status status) {
	if(status != NVAPI_OK) {
		NvAPI_ShortString desc = {0};
		NvAPI_GetErrorMessage(status, desc);
		fprintf(stderr, "NVAPI error: %s\n", desc);
		return false;
	}
	return true;
}

void printSetting(NVDRS_SETTING const *const setting) {
	assert(setting);

	printf("[0x%08lX] ", setting->settingId);
	printf("%s\nvalue: ", UTF16ToUTF8(setting->settingName));
	switch(setting->settingType) {
		case NVDRS_DWORD_TYPE:
			printf("0x%08lX", setting->u32CurrentValue);
			break;
		case NVDRS_BINARY_TYPE: {
			unsigned int len;
			printf("%luB [", setting->binaryCurrentValue.valueLength);
			for(len = 0; len < setting->binaryCurrentValue.valueLength; ++len) {
				printf("%02x", setting->binaryCurrentValue.valueData[len]);
			}
			printf("]");
		}
			break;
		case NVDRS_STRING_TYPE:
		case NVDRS_WSTRING_TYPE:
			printf("\"%s\"", UTF16ToUTF8(setting->wszCurrentValue));
			break;
	}
	printf(" %s\n", setting->isCurrentPredefined ? "(pred)" : "(user)");
	printf("location: ");
	switch(setting->settingLocation) {
		case NVDRS_CURRENT_PROFILE_LOCATION:
			puts("NVDRS_CURRENT_PROFILE_LOCATION");
			break;
		case NVDRS_GLOBAL_PROFILE_LOCATION:
			puts("NVDRS_GLOBAL_PROFILE_LOCATION");
			break;
		case NVDRS_BASE_PROFILE_LOCATION:
			puts("NVDRS_BASE_PROFILE_LOCATION");
			break;
		case NVDRS_DEFAULT_PROFILE_LOCATION:
			puts("NVDRS_DEFAULT_PROFILE_LOCATION");
			break;
	}
	puts("\n");
}

bool setVSyncTriplet(NvDRSSessionHandle const hSession, NvDRSProfileHandle const hProfile, VSyncTriplet vSyncTriplet) {
	assert(hSession);
	assert(hProfile);

	if(vSyncTriplet.vSync.settingId == (NvU32)INVALID_SETTING_ID || vSyncTriplet.vSyncTearControl.settingId == (NvU32)INVALID_SETTING_ID || vSyncTriplet.AFR.settingId == (NvU32)INVALID_SETTING_ID) {
		// invalid triplet
		return true;
	}
	if(vSyncTriplet.vSync.version == MAGIC_NVDRS_SETTING_VER && vSyncTriplet.vSyncTearControl.version == MAGIC_NVDRS_SETTING_VER && vSyncTriplet.AFR.version == MAGIC_NVDRS_SETTING_VER) {
		// magic triplet, correct here
		vSyncTriplet.vSync.version = NVDRS_SETTING_VER;
		vSyncTriplet.vSyncTearControl.version = NVDRS_SETTING_VER;
		vSyncTriplet.AFR.version = NVDRS_SETTING_VER;
	}
	bool status1 = true, status2 = true, status3 = true;
	if(vSyncTriplet.vSync.version) {
		status1 = nvapiExceptionPrint(NvAPI_DRS_SetSetting(hSession, hProfile, (NVDRS_SETTING *)&vSyncTriplet.vSync));
	} else {
		NvAPI_Status const status = NvAPI_DRS_RestoreProfileDefaultSetting(hSession, hProfile, vSyncTriplet.vSync.settingId);
		if(status != NVAPI_SETTING_NOT_FOUND) {
			status1 = nvapiExceptionPrint(status);
		}
	}
	if(vSyncTriplet.vSyncTearControl.version) {
		status2 = nvapiExceptionPrint(NvAPI_DRS_SetSetting(hSession, hProfile, (NVDRS_SETTING *)&vSyncTriplet.vSyncTearControl));
	} else {
		NvAPI_Status const status = NvAPI_DRS_RestoreProfileDefaultSetting(hSession, hProfile, vSyncTriplet.vSyncTearControl.settingId);
		if(status != NVAPI_SETTING_NOT_FOUND) {
			status2 = nvapiExceptionPrint(status);
		}
	}
	if(vSyncTriplet.AFR.version) {
		status3 = nvapiExceptionPrint(NvAPI_DRS_SetSetting(hSession, hProfile, (NVDRS_SETTING *)&vSyncTriplet.AFR));
	} else {
		NvAPI_Status const status = NvAPI_DRS_RestoreProfileDefaultSetting(hSession, hProfile, vSyncTriplet.AFR.settingId);
		if(status != NVAPI_SETTING_NOT_FOUND) {
			status3 = nvapiExceptionPrint(status);
		}
	}
	if(!status1) {
		eprintf("failed to set vSync mode setting");
	}
	if(!status2) {
		eprintf("failed to set smooth AFR behavior setting");
	}
	if(!status3) {
		eprintf("failed to set VSync Tear Control setting");
	}
	return status1 && status2 && status3;
}

int setVSyncTriplets(NvDRSSessionHandle const hSession, NvDRSProfileHandle *const hProfiles, VSyncTriplet const *const vSyncTriplets, int const length) {
	assert(hSession);
	assert(hProfiles || length == 0);
	assert(vSyncTriplets || length == 0);

	int done = 0;
	for(int i = 0; i < length; ++i) {
		bool status = true;
		if(hProfiles[i]) {
			if(vSyncTriplets->vSync.version == MAGIC_NVDRS_SETTING_VER && vSyncTriplets->vSyncTearControl.version == MAGIC_NVDRS_SETTING_VER && vSyncTriplets->AFR.version == MAGIC_NVDRS_SETTING_VER) {
				// magic triplet, not array
				status = setVSyncTriplet(hSession, hProfiles[i], *vSyncTriplets);
			} else {
				status = setVSyncTriplet(hSession, hProfiles[i], vSyncTriplets[i]);
			}
		}
		if(status) {
			++done;
		}
	}
	return done;
}

bool getVSyncTriplet(NvDRSSessionHandle const hSession, NvDRSProfileHandle const hProfile, VSyncTriplet *const vSyncTriplet) {
	assert(hSession);
	assert(hProfile);
	assert(vSyncTriplet);

	*vSyncTriplet = (VSyncTriplet){0};
	vSyncTriplet->vSync.version = NVDRS_SETTING_VER;
	vSyncTriplet->vSyncTearControl.version = NVDRS_SETTING_VER;
	vSyncTriplet->AFR.version = NVDRS_SETTING_VER;
	// if NVAPI_SETTING_NOT_FOUND, settingIds are not set
	vSyncTriplet->vSync.settingId = VSYNCMODE_ID;
	vSyncTriplet->vSyncTearControl.settingId = VSYNCTEARCONTROL_ID;
	vSyncTriplet->AFR.settingId = VSYNCSMOOTHAFR_ID;
	NvAPI_Status const status1 = NvAPI_DRS_GetSetting(hSession, hProfile, VSYNCMODE_ID, &vSyncTriplet->vSync);
	NvAPI_Status const status2 = NvAPI_DRS_GetSetting(hSession, hProfile, VSYNCTEARCONTROL_ID, &vSyncTriplet->vSyncTearControl);
	NvAPI_Status const status3 = NvAPI_DRS_GetSetting(hSession, hProfile, VSYNCSMOOTHAFR_ID, &vSyncTriplet->AFR);
	bool status = true;
	if(status1 == NVAPI_SETTING_NOT_FOUND) {
		vSyncTriplet->vSync.version = 0;
	} else if(status1 != NVAPI_OK) {
		nvapiExceptionPrint(status1);
		eprintf("failed to get vSync mode setting");
		status = false;
	}
	if(status2 == NVAPI_SETTING_NOT_FOUND) {
		vSyncTriplet->vSyncTearControl.version = 0;
	} else if(status2 != NVAPI_OK) {
		nvapiExceptionPrint(status2);
		eprintf("failed to get smooth AFR behavior setting");
		status = false;
	}
	if(status3 == NVAPI_SETTING_NOT_FOUND) {
		vSyncTriplet->AFR.version = 0;
	} else if(status3 != NVAPI_OK) {
		nvapiExceptionPrint(status3);
		eprintf("failed to get VSync Tear Control setting");
		status = false;
	}
	if(!status) {
		// invalid triplet
		vSyncTriplet->vSync.settingId = (NvU32)INVALID_SETTING_ID;
		vSyncTriplet->vSyncTearControl.settingId = (NvU32)INVALID_SETTING_ID;
		vSyncTriplet->AFR.settingId = (NvU32)INVALID_SETTING_ID;
		return false;
	}
	if(vSyncTriplet->vSync.settingLocation != NVDRS_CURRENT_PROFILE_LOCATION || vSyncTriplet->vSync.isCurrentPredefined) {
		vSyncTriplet->vSync.version = 0;
	}
	if(vSyncTriplet->vSyncTearControl.settingLocation != NVDRS_CURRENT_PROFILE_LOCATION || vSyncTriplet->vSyncTearControl.isCurrentPredefined) {
		vSyncTriplet->vSyncTearControl.version = 0;
	}
	if(vSyncTriplet->AFR.settingLocation != NVDRS_CURRENT_PROFILE_LOCATION || vSyncTriplet->AFR.isCurrentPredefined) {
		vSyncTriplet->AFR.version = 0;
	}
	return true;
}

int getVSyncTriplets(NvDRSSessionHandle const hSession, NvDRSProfileHandle const *const hProfiles, VSyncTriplet *const vSyncTriplets, int const length) {
	assert(hSession);
	assert(hProfiles || length == 0);
	assert(vSyncTriplets || length == 0);

	int done = 0;
	for(int i = 0; i < length; ++i) {
		if(hProfiles[i] == NULL || getVSyncTriplet(hSession, hProfiles[i], &vSyncTriplets[i])) {
			++done;
		}
	}
	return done;
}

int getAppsHProfiles(NvDRSSessionHandle const hSession, WCHAR const *const *const paths, NvDRSProfileHandle *const hAppProfiles, int const length) {
	assert(hSession);
	assert(paths || length == 0);
	assert(hAppProfiles || length == 0);

	int done = 0;
	for(int i = 0; i < length; ++i) {
		NVDRS_APPLICATION app = {0};
		app.version = NVDRS_APPLICATION_VER;
		NvAPI_Status status = NvAPI_DRS_FindApplicationByName(hSession, (WCHAR *)paths[i], &hAppProfiles[i], &app);
		switch(status) {
			case NVAPI_EXECUTABLE_NOT_FOUND:
				printf("warn: \"%s\" doesn't have an associated profile\n", UTF16ToUTF8(paths[i]));
				hAppProfiles[i] = NULL;
			case NVAPI_OK:
				++done;
				break;
			case NVAPI_EXECUTABLE_PATH_IS_AMBIGUOUS:
				nvapiExceptionPrint(status);
				eprintf("skipped ambiguous path: \"%s\"", UTF16ToUTF8(paths[i]));
				hAppProfiles[i] = NULL;
				break;
			default:
				nvapiExceptionFatal(status);
		}
	}
	return done;
}

void printHelp(void) {
	puts("vsync.exe [command] [-Force] [executables]... \nCommands:");
	puts("set\t\tenables vsync");
	puts("restore\t\trestore previous vsync settings");
	puts("restore -Force\toverwrite settings file\n");
	puts("explicit executable paths are only required if the associated profile contains vSync options\n");
}

Mode parseArg(int const argc, WCHAR const *const *const argv) {
	if(argc >= 2) {
		if(wcscmp(argv[1], L"set") == 0) {
			if(argc >= 3 && wcscmp(argv[2], L"-Force") == 0) {
				++numStaticArgs;
				return MODE_FORCE_SET;
			}
			return MODE_SET;
		}
		if(wcscmp(argv[1], L"restore") == 0) {
			return MODE_RESTORE;
		}
	}
	eprintf("invalid arguments");
	return MODE_HELP;
}

static NvDRSSessionHandle hSession;

void NvAPI_UnloadExitHandler(void) {
	nvapiExceptionPrint(NvAPI_Unload());
}

void NvAPI_DRS_DestroySessionExitHandler(void) {
	nvapiExceptionPrint(NvAPI_DRS_DestroySession(hSession));
}

int wmain(int argc, wchar_t **argv) {
	setlocale(LC_ALL, ".UTF8");
	_set_error_mode(_OUT_TO_STDERR);
	assert(NVDRS_SETTING_VER != 0);

	Mode const mode = parseArg(argc, (WCHAR const *const *)argv);

	if(mode == MODE_HELP) {
		printHelp();
		return 0;
	}

	// initialize NVAPI. This must be done first of all
	nvapiExceptionFatal(NvAPI_Initialize());
	atexit(NvAPI_UnloadExitHandler);
	nvapiExceptionFatal(NvAPI_DRS_CreateSession(&hSession));
	atexit(NvAPI_DRS_DestroySessionExitHandler);

	int const appArgc = argc-numStaticArgs-1;
	VSyncTriplet globalVSyncTriplet = vSyncOn;
	VSyncTriplet const *appVSyncTriplets = &vSyncOn;

	if(mode == MODE_RESTORE) {
		if(!nvapiExceptionPrint(NvAPI_DRS_LoadSettingsFromFile(hSession, L"settings"))) {
			eprintf("failed to retrieve settings backup");
			exit(-1);
		}
		NvDRSProfileHandle globalProfile;
		nvapiExceptionFatal(NvAPI_DRS_GetCurrentGlobalProfile(hSession, &globalProfile));
		if(!getVSyncTriplet(hSession, globalProfile, &globalVSyncTriplet)) {
			eprintf("failed to retrieve global profile from backup");
			exit(-1);
		}

		NvDRSProfileHandle *const hAppProfiles = malloc((unsigned)appArgc*sizeof(NvDRSProfileHandle));
		VSyncTriplet *tempVSyncTriplets = malloc((unsigned)appArgc*sizeof(VSyncTriplet));
		int done = getAppsHProfiles(hSession, (WCHAR const *const *)(argv+numStaticArgs+1), hAppProfiles, appArgc);
		done -= appArgc-getVSyncTriplets(hSession, hAppProfiles, tempVSyncTriplets, appArgc);
		free(hAppProfiles);
		appVSyncTriplets = tempVSyncTriplets;
		if(done != appArgc) {
			eprintf("successfully retrieved %d/%d apps from backup", done, appArgc);
		}
		if(_wremove(BACKUP_FILENAME)) {
			eprintf("failed to delete settings file");
		}
	}
	nvapiExceptionFatal(NvAPI_DRS_LoadSettings(hSession));
	if(mode&MODE_SET) {
		if(_waccess(BACKUP_FILENAME, 0) || mode == MODE_FORCE_SET) {
			nvapiExceptionFatal(NvAPI_DRS_SaveSettingsToFile(hSession, BACKUP_FILENAME));
		} else {
			printf("backup already exists, skipping\n");
		}
	}
	NvDRSProfileHandle globalProfile;
	nvapiExceptionFatal(NvAPI_DRS_GetCurrentGlobalProfile(hSession, &globalProfile));
	if(!setVSyncTriplet(hSession, globalProfile, globalVSyncTriplet)) {
		eprintf("failed to set global profile settings");
	}

	NvDRSProfileHandle *const hAppProfiles = malloc((unsigned)appArgc*sizeof(NvDRSProfileHandle));
	int done = getAppsHProfiles(hSession, (WCHAR const *const *)(argv+numStaticArgs+1), hAppProfiles, appArgc);
	done -= appArgc-setVSyncTriplets(hSession, hAppProfiles, appVSyncTriplets, appArgc);
	free(hAppProfiles);
	if(done != appArgc) {
		eprintf("successfully set %d/%d apps", done, appArgc);
	}

	if(mode == MODE_RESTORE) {
		free((VSyncTriplet *)appVSyncTriplets);
	}

	nvapiExceptionFatal(NvAPI_DRS_SaveSettings(hSession));
	return 0;
}

#ifdef __CLION_IDE__
	#pragma clang diagnostic pop
#endif
