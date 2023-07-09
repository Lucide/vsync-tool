# VSync Override Tool

A small utility to set and restore vSync settings. Useful when [sunshine](https://github.com/LizardByte/Sunshine) requires Fast Sync off.

```txt
vsync.exe [command] [-Force] [executables]...
Commands:
set             enables vsync
restore         restore previous vsync settings
restore -Force  overwrite settings file

explicit executable paths are only required if the associated profile contains vSync options
```

## NvAPI notes

NvAPI DRS documentation sucks. My observations:

* `nvapi.h` requires `windows.h` to be included beforehand.
* In principle, `NvAPI_DRS_GetCurrentGlobalProfile()` ≠ `NvAPI_DRS_GetBaseProfile()`. In fact, the current global profile can't be set from the Nvidia Control Panel.
* `NvAPI_DRS_FindApplicationByName()` can return `NVAPI_EXECUTABLE_NOT_FOUND`, not `NVAPI_APPLICATION_NOT_FOUND`, which does not exist.
* `NVDRS_PROFILE.isPredefined` can be `true` for custom app profiles.
* Getter functions like `NvAPI_DRS_GetSetting()` can leave spurious or outright wrong data if the input struct hasn't been zeroed properly.
* profile names should be unique (untested).
* `NvAPI_DRS_RestoreProfileDefaultSetting()` seems to return `NVAPI_SETTING_NOT_FOUND` when it shouldn't.

### `NVDRS_SETTING.settingLocation` values:

| value                            | meaning                                                                                                                                                                                |
|----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NVDRS_DEFAULT_PROFILE_LOCATION` | implicit default global driver settings. `NvAPI_DRS_GetSetting()` returns `NVAPI_SETTING_NOT_FOUND`, and `settingId` is not set                                                        |
| `NVDRS_BASE_PROFILE_LOCATION`    | settings inherited from the Base Profile. Available only if the current global profile is not the Base Profile. The Base Profile is always shown in the Nvidia Control Panel           |
| `NVDRS_GLOBAL_PROFILE_LOCATION`  | settings inherited from the current global profile, overrides `NVDRS_BASE_PROFILE_LOCATION` if current global profile is the Base Profile (can't be changed from nvidia control panel) |
| `NVDRS_CURRENT_PROFILE_LOCATION` | settings in the current profile handle.  If it's an app profile, `profileInformation.isPredefined` tells if the settings is a driver app setting or a user setting                     |
