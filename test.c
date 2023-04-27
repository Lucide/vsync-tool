#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <locale.h>
#include "nvapi.h"
#include "NvApiDriverSettings.h"

#define ERROR_HEADER "error: "
#define eprintf(STR, ...) fprintf(stderr, ERROR_HEADER STR "\n", ##__VA_ARGS__)

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
	printf("[0x%08lX] ", setting->settingId);
	printf("%.20ls\nvalue: ", setting->settingName);
	switch(setting->settingType) {
		case NVDRS_DWORD_TYPE:
			printf("0x%08lX", setting->u32CurrentValue);
			break;
		case NVDRS_BINARY_TYPE: {
			unsigned int len;
			printf("[bin]");
		}
			break;
		case NVDRS_STRING_TYPE:
		case NVDRS_WSTRING_TYPE:
			printf("\"%.8ls\"", setting->wszCurrentValue);
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
	// puts("");
}

void getAppsHProfiles(NvDRSSessionHandle const hSession, WCHAR const *const *const paths, int const pathsLength, NvDRSProfileHandle *const hAppProfiles, int *const hAppProfilesLength) {
	*hAppProfilesLength = 0;
	for(int i = 0; i < pathsLength; ++i) {
		NVDRS_APPLICATION app = {
				.version=NVDRS_APPLICATION_VER
		};
		assert((void *)&app == (void *)&app.version);
		NvAPI_Status status = NvAPI_DRS_FindApplicationByName(hSession, (WCHAR *)paths[i], &hAppProfiles[*hAppProfilesLength], &app);
		switch(status) {
			case NVAPI_OK:
				++*hAppProfilesLength;
				break;
			case NVAPI_EXECUTABLE_NOT_FOUND:
				printf("info: \"%ls\" doesn't have a custom profile\n", paths[i]);
				break;
			case NVAPI_EXECUTABLE_PATH_IS_AMBIGUOUS:
				nvapiExceptionPrint(status);
				eprintf("ambiguous path: \"%ls\"", paths[i]);
				break;
			default:
				nvapiExceptionFatal(status);
		}
	}
}

void PrintError(NvAPI_Status status) {
	NvAPI_ShortString szDesc = {0};
	NvAPI_GetErrorMessage(status, szDesc);
	printf(" NVAPI error: %s\n", szDesc);
	exit(-1);
}

bool DisplayProfileContents(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile) {
	// (0) this function assumes that the hSession and hProfile are
	// valid handles obtained from nvapi.
	NvAPI_Status status;
	// (1) First, we retrieve generic profile information
	// The structure will provide us with name, number of applications
	// and number of settings for this profile.
	NVDRS_PROFILE profileInformation = {0};
	profileInformation.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(hSession, hProfile, &profileInformation);
	if(status != NVAPI_OK) {
		PrintError(status);
		return false;
	}
	printf("Profile Name: %ls\n", profileInformation.profileName);
	printf("Number of Applications associated with the Profile: %d\n", profileInformation.numOfApps);
	printf("Number of Settings associated with the Profile: %d\n", profileInformation.numOfSettings);
	printf("Is Predefined: %d\n", profileInformation.isPredefined);

	{
		NVDRS_SETTING setting = {0};
		setting.version = NVDRS_SETTING_VER;
		nvapiExceptionPrint(NvAPI_DRS_GetSetting(hSession, hProfile, PRERENDERLIMIT_ID, &setting));
		printSetting(&setting);
	}
	{
		NVDRS_SETTING setting = {0};
		setting.version = NVDRS_SETTING_VER;
		nvapiExceptionPrint(NvAPI_DRS_GetSetting(hSession, hProfile, OGL_TRIPLE_BUFFER_ID, &setting));
		printSetting(&setting);
	}
	puts("listed-------\\/");

	// (4) Now we enumerate all the settings on the profile
	if(profileInformation.numOfSettings > 0) {
		NVDRS_SETTING *setArray = malloc(profileInformation.numOfSettings*sizeof(NVDRS_SETTING));
		NvU32 numSetRead = profileInformation.numOfSettings, i;
		// (5) The function to retrieve the settings in a profile works
		// like the function to retrieve the applications.
		setArray[0].version = NVDRS_SETTING_VER;
		status = NvAPI_DRS_EnumSettings(hSession, hProfile, 0, &numSetRead, setArray);
		if(status != NVAPI_OK) {
			PrintError(status);
			return false;
		}
		for(i = 0; i < numSetRead; i++) {
			if(setArray[i].settingLocation != NVDRS_CURRENT_PROFILE_LOCATION) {
				// (6) The settings that are not from the Current Profile
				// are inherited from the Base or Global profiles. Skip them.
				// continue;
			}
			printSetting(&setArray[i]);
		}
	}
	printf("\n");
	return true;
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

	// initialize NVAPI. This must be done first of all
	nvapiExceptionFatal(NvAPI_Initialize());
	atexit(NvAPI_UnloadExitHandler);
	nvapiExceptionFatal(NvAPI_DRS_CreateSession(&hSession));
	atexit(NvAPI_DRS_DestroySessionExitHandler);

	nvapiExceptionFatal(NvAPI_DRS_LoadSettings(hSession));

	int const appArgc = argc-1;

	NvDRSProfileHandle profile;
	nvapiExceptionFatal(NvAPI_DRS_GetBaseProfile(hSession, &profile));
	DisplayProfileContents(hSession, profile);

	nvapiExceptionFatal(NvAPI_DRS_GetCurrentGlobalProfile(hSession, &profile));
	DisplayProfileContents(hSession, profile);

	nvapiExceptionFatal(NvAPI_DRS_FindProfileByName(hSession, L"GLOBTEST", &profile));
	DisplayProfileContents(hSession, profile);

	// nvapiExceptionFatal(NvAPI_DRS_SetCurrentGlobalProfile(hSession, L"Base Profile"));
	// nvapiExceptionFatal(NvAPI_DRS_GetBaseProfile(hSession, &profile));
	// DisplayProfileContents(hSession, profile);

	int length = appArgc;
	NvDRSProfileHandle *const hAppProfiles = malloc((unsigned)length*sizeof(NvDRSProfileHandle));
	getAppsHProfiles(hSession, (WCHAR const *const *)(argv+1), appArgc, hAppProfiles, &length);
	DisplayProfileContents(hSession, hAppProfiles[0]);

	nvapiExceptionFatal(NvAPI_DRS_SaveSettings(hSession));

	return 0;
}
