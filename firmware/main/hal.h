#ifndef AGR_HAL_H
#define AGR_HAL_H

//firmware update app
//this needs to be uncommented to build the firmware update app.
//fwupdate should be flashed only to the factory partition, never to ota_0!
//fwupdate looks for new firmware in /.mega/firmware.mgf (for updates selected from filebrowser) or /factory.mgf (for initial flashes)
//#define FWUPDATE



//MegaGRRL hardware defs
//this needs to be updated based on which hardware version is being targeted in the build.

//MegaGRRL Portable, ver 2.1
#define HWVER_PORTABLE

//MegaGRRL Desktop, ver 1.0
//#define HWVER_DESKTOP

#endif